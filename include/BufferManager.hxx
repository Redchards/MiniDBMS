#ifndef BUFFER_MANAGER_HXX
#define BUFFER_MANAGER_HXX

#include <Configuration.hxx>
#include <RawDataUtils.hxx>
#include <DataTypes.hxx>
#include <DiskPage.hxx>
#include <LRUPageReplacePolicy.hxx>
#include <MetaUtils.hxx>
#include <PageReader.hxx>
#include <PageWriter.hxx>
#include <ResourceHandler.hxx>

#include <mutex>
#include <thread>
#include <vector>

enum class PageType : uint8_t
{
	ReadOnly,
	Writable
};

class DoublePageFreeException : public std::exception
{
public:
	DoublePageFreeException(const std::string& msg) : msg_{std::string{"Error when freeing disk page from buffer : "} + msg}
	{}

	const char* what() const noexcept override
	{
		return msg_.c_str();
	}

private:
	const std::string msg_;
};

template<Endianness>
class BufferManager;

template<Endianness endian, PageType type>
class BufferedPageStrategy
{
public:
	struct ConstHandle
	{
		const BufferManager<endian>* manager;
		typename BufferManager<endian>::PageIndex pageId;
	};

	struct Handle
	{
		BufferManager<endian>* manager;
		typename BufferManager<endian>::PageIndex pageId;
	};

	using HandleType = std::conditional_t<type == PageType::ReadOnly,
										  ConstHandle,
										  Handle>;

	static HandleType construct(decltype(std::declval<HandleType>().manager) manager, decltype(std::declval<HandleType>().pageId) pageId) noexcept 
	{
		manager->pin(pageId);
		return {manager, pageId};
	}

	static void destroy(optional<HandleType> handle)
	{
		if(handle)
		{
			handle->manager->unpin(handle->pageId);
		}
		else
		{
			throw DoublePageFreeException("The handle is no longer valid");
		}
	}  
};

template<Endianness endian, PageType type>
struct BufferedPageHandle : public ResourceHandler<BufferedPageStrategy<endian, type>>
{
	using Base = ResourceHandler<BufferedPageStrategy<endian, type>>;

public:
	using Base::Base;

	auto get() const noexcept
	{
		using ManagerType = std::remove_pointer_t<decltype(Base::get()->manager)>;
		using PageIndex = typename ManagerType::PageIndex;
		using PageType = decltype(std::declval<ManagerType>().template getPageFromIndex<type>(std::declval<PageIndex>()));

		if(Base::get())
		{
			auto pageId = Base::get()->pageId;


			return optional<PageType>{Base::get()->manager->template getPageFromIndex<type>(pageId)};
		}

		return optional<PageType>{};
	}
};

template<Endianness endian>
class BufferManager
{
	static constexpr size_type defaultBufferSize = 4096;
	static constexpr size_type pageSize = 512;

	public:
	using PageIndex = size_type;
	using DefaultPolicy = LRUPageReplacePolicy<endian>;

	struct DiskPageDescriptor
	{
		DiskPageDescriptor(DiskPage<endian>&& page_, std::streamoff offset_)
		: page{std::move(page_)},
		  offset{offset_}
		{}

		DiskPage<endian> page;
		std::streamoff offset;
	};

	public:
	BufferManager(const std::string& dbFileName)
	: bufferPool_{},
	  pinCountList_{},
	  replacePolicy_{new DefaultPolicy()},
	  bufferPagePosition_{},
	  pgReader_{dbFileName},
	  pgWriter_{dbFileName},
	  bufferSize_{defaultBufferSize}
	{
		bufferPool_.reserve(bufferSize_);
	}

	BufferManager(const std::string& dbFileName, size_type bufferSize)
	: bufferPool_{},
	  pinCountList_{},
	  replacePolicy_{new DefaultPolicy()},
	  bufferPagePosition_{},
	  pgReader_{dbFileName},
	  pgWriter_{dbFileName},
	  bufferSize_{bufferSize}
	{
		bufferPool_.reserve(bufferSize_);
	}

	/* Functions to pin and unpin pages.
	 * Using simple mutex lock to ensure the thread-safe
	 * character of this action (these can effectively be called from multiple threads/process).
     * (Disabled for now)
     */
	void pin(PageIndex pageId) noexcept
	{
		if(pageId < bufferPool_.size())
		{
			/*{
				std::lock_guard<std::mutex> lock{mut_}; */
			std::cout << "pin" << std::endl;
			if(pageId >= pinCountList_.size())
			{
				pinCountList_.resize(pageId + 1);
			}

			pinCountList_[pageId] += 1;
			std::cout << replacePolicy_.get() << std::endl;
			replacePolicy_->use(getPageFromIndex<PageType::ReadOnly>(pageId));
			/*}*/
		}
	}

	void unpin(PageIndex pageId) noexcept
	{
		//std::lock_guard<std::mutex> lock{mut_};
		std::cout << "unpin" << std::endl;
		size_type pinCount = pinCountList_[pageId]; 
		if((pageId < bufferPool_.size()) && (pinCount > 0))
		{
			pinCountList_[pageId] -= 1;
		}
		if(pinCount == 0)
		{
			replacePolicy_->release(getPageFromIndex<PageType::ReadOnly>(pageId));
		}
	}

	// Maybe add a setter too ?
	size_type getBufferSize() const noexcept
	{
		return bufferSize_;
	}

	template<PageType type>
	BufferedPageHandle<endian, type> requestFreePage(const DbSchema& schema)
	{
		// Look if we have any known available page.
		auto candidatePageOffset = firstAvailablePageOffsetMap_.find(schema.getName());

		using HandleType = BufferedPageHandle<endian, type>;

		// If we do, then proceed to retrieve it from the buffer or fetch it from the file if it's not in buffer.
		if(candidatePageOffset != firstAvailablePageOffsetMap_.end())
		{
			std::cout << "page" << std::endl;
			auto pos = bufferPagePosition_.find(candidatePageOffset->second);
			
			if(pos != bufferPagePosition_.end())
			{
				DiskPageDescriptor& pageDescriptor = bufferPool_[pos->second];
				if(!pageDescriptor.page.isFull()) return HandleType::create(this, pos->second);
			}
			else
			{
				auto newPageDescriptor = fetchNewPageIfFree(schema.getName(), candidatePageOffset->second);
				if(newPageDescriptor)
				{
					return HandleType::create(this, newPageDescriptor->page.getIndex());
				}
			}
		}
		// If no available page is known, then look for a free page in the file.
		// If we find any, fetch it and return it.
		// Else, just return nullopt and let the system create a new page if needed.
		else
		{
			std::cout << "no page" << std::endl;
			auto offset = lookForFirstFreePage(schema.getName());
			if(offset)
			{
				return HandleType::create(this, fetchNewPage(schema.getName(), *offset).page.getIndex());
			}
		}

		return {};
	}

	// Maybe put private and allow just friend BufferedPageStrategy to use ?
	template<PageType type>
	auto getPageFromIndex(PageIndex pageId) noexcept
	-> std::conditional_t<type == PageType::ReadOnly, const DiskPage<endian>&, DiskPage<endian>&>
	{
		return bufferPool_[pageId].page;
	}

	template<PageType type>
	const DiskPage<endian>& getPageFromIndex(PageIndex pageId) const noexcept
	{
		return bufferPool_[pageId].page;
	}

	optional<DiskPage<endian>&> getNext(std::streamoff offset)
	{
		
	}

	/*template<>
	const DiskPage& requestPage<PageType::NonModifiable>(const std::string& schemaName)
	{
		return requestPageAux(schemaName);
	}*/

	// TODO : Add a "clearCache" function to remove extraneous pages

	private:

	optional<PageIndex> performPageReplace(const std::string& schemaName, std::streamoff newPageOffset)
	{
		auto candidatePageId = replacePolicy_->getCandidate(schemaName);

		if(candidatePageId)
		{
			auto oldPage = bufferPool_[*candidatePageId];

			bufferPagePosition_.erase(oldPage.offset);

			if(oldPage.page.isDirty())
			{
				pgWriter_.writePage(oldPage.page, oldPage.offset);
			}

			bufferPagePosition_.insert({newPageOffset, *candidatePageId});
			bufferPool_[*candidatePageId] = {pgReader_.readPage(*candidatePageId, newPageOffset), newPageOffset};
		}
		
		return candidatePageId;
	}

	DiskPageDescriptor& fetchNewPage(const std::string& schemaName, std::streamoff offset)
	{
		// If we still have room in buffer, just fetch the page and place it at the end.
		if(bufferPool_.size() < getBufferSize())
		{
			bufferPool_.emplace_back(pgReader_.readPage(bufferPool_.size(), offset), offset);
			bufferPagePosition_[offset] = bufferPool_.size() - 1;
			return bufferPool_.back();
		}
		else
		{
			// Else, try to perform a page replace. If it fails, just extend the buffer and put it at the end.
			auto replacedPageId = performPageReplace(schemaName, offset);
			if(replacedPageId)
			{
				return bufferPool_[*replacedPageId];
			}
			else
			{
				bufferPool_.emplace_back(pgReader_.readPage(bufferPool_.size(), offset), offset);
				bufferPagePosition_[offset] = bufferPool_.size() - 1;
				return bufferPool_.back();
			}
		}

		return bufferPool_.back();
	}

	optional<DiskPageDescriptor&> fetchNewPageIfFree(const std::string& schemaName, std::streamoff offset)
	{
		if((bufferPool_.size() < getBufferSize()) && !pgReader_.readPageHeader(offset).isFull())
		{
			std::cout << "here" << std::endl;
			bufferPool_.emplace_back(pgReader_.readPage(bufferPool_.size(), offset), offset);
			bufferPagePosition_[offset] = bufferPool_.size() - 1;
			return bufferPool_.back();
		}
		else
		{
			std::cout << "heredd" << std::endl;
			auto replacedPageId = performPageReplace(schemaName, offset);
			if(replacedPageId && !bufferPool_[*replacedPageId].page.isFull())
			{
				return bufferPool_[*replacedPageId];
			}
		}

		return {};
	}

	optional<std::streamoff> lookForFirstFreePage(const std::string& schemaName)
	{
		auto it = firstPageOffsetMap_.find(schemaName);

		if(it == firstPageOffsetMap_.end())
		{
			if(lookForFirstPage(schemaName))
			{
				it = firstPageOffsetMap_.find(schemaName);
			}
			else
			{
				return {};
			}
		}

		auto offset = it->second;
		std::cout << offset << std::endl;

		while(true)
		{
			DiskPageHeader<endian> header = pgReader_.readPageHeader(offset);
			std::cout << header.getNextPageOffset() << std::endl;
			if(!header.isFull())
			{
				return offset;
			}
			else if(header.getNextPageOffset() == 0)
			{
				return {};
			}

			offset = header.getNextPageOffset();
		}
	}

	// A nullopt in return means that there is no page which contains this type of schema in the DB
	optional<std::streamoff> lookForFirstPage(const std::string& schemaName)
	{
		auto it = firstPageOffsetMap_.find(schemaName);

		if(it != firstPageOffsetMap_.end())
		{
			return it->second;
		}
		else
		{
			std::streamoff offset = 0;

			while(true)
			{
				DiskPageHeader<endian> header = pgReader_.readPageHeader(offset);
				if(header.getSchemaName() == schemaName)
				{
					firstPageOffsetMap_[schemaName] = offset;
					return offset;
				}

				offset += header.getRawPageSize();
				if(offset >= pgReader_.getFileSize())
				{
					return {};
				}
			}
		}
	}

	std::vector<DiskPageDescriptor> bufferPool_;
	std::vector<size_type> pinCountList_;
	std::unique_ptr<PageReplacePolicy<endian>> replacePolicy_;
	std::unordered_map<std::streamoff, size_type> bufferPagePosition_;

	/* The page at the offset contained in this map may or may not be full, they are the last "non-full" page that we're aware of.
	 * However, they may be no such page, in which case firstAvailablePageOffset_.find(schemaName) will return the "end" iterator */
	std::unordered_map<std::string, std::streamoff> firstAvailablePageOffsetMap_;
	std::unordered_map<std::string, std::streamoff> firstPageOffsetMap_;

	PageReader<endian> pgReader_;
	PageWriter<endian> pgWriter_;

	size_type bufferSize_;
};

#endif // BUFFER_MANAGER_HXX
