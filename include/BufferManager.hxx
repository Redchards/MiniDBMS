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
	struct HandleType
	{
		BufferManager<endian>* manager;
		typename BufferManager<endian>::PageIndex pageId;

		friend bool operator==(HandleType lhs, HandleType rhs);
		friend bool operator!=(HandleType lhs, HandleType rhs);
	};

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
bool operator==(typename BufferedPageStrategy<endian, type>::HandleType lhs, typename BufferedPageStrategy<endian, type>::HandleType rhs)
{
	return (lhs.manager == rhs.manager) && (lhs.pageId == rhs.pageId);
}

template<Endianness endian, PageType type>
bool operator!=(typename BufferedPageStrategy<endian, type>::HandleType lhs, typename BufferedPageStrategy<endian, type>::HandleType rhs)
{
	return !(lhs == rhs);
}

template<Endianness endian>
class BufferManager;

template<Endianness endian, PageType type>
struct BufferedPageHandle : public ResourceHandler<BufferedPageStrategy<endian, type>>
{
	using Base = ResourceHandler<BufferedPageStrategy<endian, type>>;

public:
	using Base::Base;

	auto get() noexcept
	{
		using ManagerType = std::remove_pointer_t<decltype(Base::get()->manager)>;
		using PageIndex = typename ManagerType::PageIndex;
		using DiskPageType = std::conditional_t<type == PageType::ReadOnly, const DiskPage<endian>&, DiskPage<endian>&>;

		if(Base::get())
		{
			auto pageId = Base::get()->pageId;

			return optional<DiskPageType>{Base::get()->manager->bufferPool_[pageId].page};
		}

		return optional<DiskPageType>{};
	}

	auto get() const noexcept
	{
		using ManagerType = std::remove_pointer_t<decltype(Base::get()->manager)>;
		using PageIndex = typename ManagerType::PageIndex;

		if(Base::get())
		{
			auto pageId = Base::get()->pageId;

			return optional<const DiskPage<endian>&>{Base::get()->manager->bufferPool_[pageId].page};
		}

		return optional<const DiskPage<endian>&>{};
	}
};

template<Endianness endian>
class BufferManager
{
	static constexpr size_type defaultBufferSize = 512;
	static constexpr size_type pageSize = 512;

	friend class BufferedPageHandle<endian, PageType::ReadOnly>;
	friend class BufferedPageHandle<endian, PageType::Writable>;

	public:
	using PageIndex = size_type;
	using DefaultPolicy = LRUPageReplacePolicy<endian>;

	struct DiskPageDescriptor
	{
		DiskPageDescriptor(DiskPage<endian>&& page_, std::streamoff offset_)
		: page{std::move(page_)},
		  offset{offset_}
		{
			std::cout << "Blouh : " << page_.getRawPageSize() << " : " << page.getPageSize() << std::endl;
		}

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

	~BufferManager()
	{
		for(auto descriptor : bufferPool_)
		{
			if(descriptor.page.isDirty())
			{
				std::cout << "Write " << std::endl;
				pgWriter_.writePage(descriptor.page, descriptor.offset);
			}
		}
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
			replacePolicy_->use(bufferPool_[pageId].page);
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
		if(pinCountList_[pageId] == 0)
		{
			replacePolicy_->release(bufferPool_[pageId].page);
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
			std::cout << "Watch me " << std::endl;
		// If we do, then proceed to retrieve it from the buffer or fetch it from the file if it's not in buffer.
		if(candidatePageOffset != firstAvailablePageOffsetMap_.end())
		{			std::cout << "Watch me " << std::endl;
			auto pos = bufferPagePosition_.find(candidatePageOffset->second);
			
			if(pos != bufferPagePosition_.end())
			{
				std::cout << "Alreayd in buffer" << std::endl;
				DiskPageDescriptor& pageDescriptor = bufferPool_[pos->second];
				if(!pageDescriptor.page.isFull()) return HandleType::create(this, pos->second);
			}
			else
			{
				std::cout << "Fetchou" << std::endl;
				auto newPageDescriptor = fetchNewPageIfFree(candidatePageOffset->second);
				if(newPageDescriptor)
				{
					return HandleType::create(this, newPageDescriptor->page.getIndex());
				}
			}
		}
		// If no available page is known, then look for a free page in the file.
		// If we find any, fetch it and return it.
		// Else, just return nullopt and let the system create a new page if needed.
		auto offset = lookForFirstFreePage(schema.getName());
		if(offset)
		{
			std::cout << "Watch me " << std::endl;
			return HandleType::create(this, fetchNewPage(*offset).page.getIndex());
		}
		std::cout << "Hel : " << bufferPool_.size() << std::endl;
			std::cout << "Watch me " << std::endl;
		return {};
	}

	// Maybe put private and allow just friend BufferedPageStrategy to use ?
	template<PageType type>
	BufferedPageHandle<endian, type> getPageFromIndex(PageIndex pageId) noexcept
	{
		using HandleType = BufferedPageHandle<endian, type>;
		return HandleType::create(this, bufferPool_[pageId].page.getIndex());
	}

	template<PageType type>
	BufferedPageHandle<endian, type> requestPage(std::streamoff offset)
	{
		auto pagePosition = bufferPagePosition_.find(offset);

		using HandleType = BufferedPageHandle<endian, type>;

		if(pagePosition != bufferPagePosition_.end())
		{
			return HandleType::create(this, bufferPool_[pagePosition->second].page.getIndex());
		}
	
		return HandleType::create(this, fetchNewPage(offset).page.getIndex());
	}

	template<PageType type>
	BufferedPageHandle<endian, type> requestFirstPage(const std::string& schemaName)
	{
		auto offset = firstPageOffsetMap_.find(schemaName);

		using HandleType = BufferedPageHandle<endian, type>;

		if(offset != firstPageOffsetMap_.end())
		{
			auto pagePosition = bufferPagePosition_.find(offset->second);

			if(pagePosition != bufferPagePosition_.end())
			{
				return HandleType::create(this, bufferPool_[pagePosition->second].page.getIndex());
			}

			return HandleType::create(this, fetchNewPage(offset->second).page.getIndex());
		}
		else
		{
			auto offset = lookForFirstPage(schemaName);

			if(offset)
			{
				return HandleType::create(this, fetchNewPage(*offset).page.getIndex());
			}
		}

		return {};
	
	}

	template<PageType type>
	BufferedPageHandle<endian, type> requestNextPage(const DiskPage<endian>& page)
	{
		// We do not have a next page, return a void handle.
		if(page.getNextPageOffset() == 0) return {};
		auto pagePosition = bufferPagePosition_.find(page.getNextPageOffset());

		using HandleType = BufferedPageHandle<endian, type>;

		if(pagePosition != bufferPagePosition_.end())
		{
			return HandleType::create(this, bufferPool_[pagePosition->second].page.getIndex());
		}
		return HandleType::create(this, fetchNewPage(page.getNextPageOffset()).page.getIndex());
	}

	template<PageType type>
	optional<std::streamoff> getFirstPageOffset(const DbSchema& schema)
	{
		auto it = firstPageOffsetMap_.find(schema.getName());

		if(it != firstPageOffsetMap_.end())
		{
			return it->second;
		}
		else
		{
			
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

			// If there is no page in file, we have no chance to find what we're looking for
			pgReader_.rewind();
			if(pgReader_.eof()) return {};

			std::streamoff offset = 0;

			while(true)
			{
			std::cout << offset << std::endl;
				DiskPageHeader<endian> header = pgReader_.readPageHeader(offset);

				if(header.getSchemaName() == schemaName)
				{
					firstPageOffsetMap_.insert({schemaName, offset});
					return offset;
				}
				std::cout << pgReader_.getFileSize() << std::endl;
				offset += header.getRawPageSize();
				if((offset + 1) >= pgReader_.getFileSize())
				{
					std::cout << "return" << std::endl;
					return {};
				}
			std::cout << "hello" << std::endl;
			}
		}
	}

	optional<std::streamoff> lookForLastPage(const std::string& schemaName)
	{
		auto firstPageOffset = lookForFirstPage(schemaName);
		std::cout << "First page offset ? : " << firstPageOffset << std::endl;
		// If there is no page in file, we have no chance to find what we're looking for
		if(!firstPageOffset) return {};

		DiskPageHeader<endian> header = pgReader_.readPageHeader(*firstPageOffset);
		std::streamoff offset = *firstPageOffset;

		while(header.getNextPageOffset() != 0)
		{
			offset = header.getNextPageOffset();
			header = pgReader_.readPageHeader(offset);
		}

		return offset;
	}

	void flush(PageIndex index)
	{
		std::cout << "Flush : " << bufferPool_.size() << std::endl;
		pgWriter_.writePage(bufferPool_[index].page, bufferPool_[index].offset);
	}

	/*template<>
	const DiskPage& requestPage<PageType::NonModifiable>(const std::string& schemaName)
	{
		return requestPageAux(schemaName);
	}*/

	// TODO : Add a "clearCache" function to remove extraneous pages

	private:

	optional<PageIndex> performPageReplace(std::streamoff newPageOffset)
	{
		auto candidatePageId = replacePolicy_->getCandidate();

		std::cout << "Try to replace page" << std::endl;
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
			std::cout << "Replaced page : " << *candidatePageId << std::endl;
		}
		
		return candidatePageId;
	}

	// Need optional as return here !
	DiskPageDescriptor& fetchNewPage(std::streamoff offset)
	{
		// If we still have room in buffer, just fetch the page and place it at the end.
		if(bufferPool_.size() < getBufferSize())
		{
			std::cout << "Still have room in buffer : " << offset << std::endl;
			bufferPool_.emplace_back(pgReader_.readPage(bufferPool_.size(), offset), offset);
			bufferPagePosition_[offset] = bufferPool_.size() - 1;
			std::cout << bufferPool_.back().page.getRawPageSize() << std::endl;
			return bufferPool_.back();
		}
		else
		{
			// Else, try to perform a page replace. If it fails, just extend the buffer and put it at the end.
			std::cout << "No more room in buffer" << std::endl;			
			auto replacedPageId = performPageReplace(offset);
			if(replacedPageId)
			{
				std::cout << "REPLAAAAAAAAAAAAAAAAAAAACE: : " << offset << std::endl;
				return bufferPool_[*replacedPageId];
			}
			else
			{
				std::cout << "Extends buffer" << std::endl;
				bufferPool_.emplace_back(pgReader_.readPage(bufferPool_.size(), offset), offset);
				bufferPagePosition_[offset] = bufferPool_.size() - 1;
				return bufferPool_.back();
			}
		}

		// Should return nulllopt
		return bufferPool_.back();
	}

	optional<DiskPageDescriptor&> fetchNewPageIfFree(std::streamoff offset)
	{
		if((bufferPool_.size() < getBufferSize()) && !pgReader_.readPageHeader(offset).isFull())
		{
			bufferPool_.emplace_back(pgReader_.readPage(bufferPool_.size(), offset), offset);
			bufferPagePosition_[offset] = bufferPool_.size() - 1;
			return bufferPool_.back();
		}
		else
		{
			auto replacedPageId = performPageReplace(offset);
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

		// If there is no page, no point in searching in file
		if(it == firstPageOffsetMap_.end()) return {};

		auto offset = it->second;
		std::cout << "Look for first free page" << std::endl;

		bool isFull;
		std::string pageSchemaName;
		std::streamoff nextPageOffset;

		while(true)
		{
			auto pagePositionInBuffer = bufferPagePosition_.find(offset);

			if(pagePositionInBuffer != bufferPagePosition_.end())
			{
				const DiskPage<endian>& page = bufferPool_[pagePositionInBuffer->second].page;
				isFull = page.isFull();
				pageSchemaName = page.getSchemaName();
				nextPageOffset = page.getNextPageOffset();
			}
			else
			{
				DiskPageHeader<endian> header = pgReader_.readPageHeader(offset);
				isFull = header.isFull();
				pageSchemaName = header.getSchemaName();
				nextPageOffset = header.getNextPageOffset();
				std::cout << header.getFreeSlotCount() << std::endl;
			}
				std::cout << " d : " << nextPageOffset << " : " << isFull << std::endl;
			if((pageSchemaName == schemaName) && !isFull)
			{
				firstAvailablePageOffsetMap_.insert({schemaName, offset});
				return offset;
			}
			else if(nextPageOffset == 0)
			{
				return {};
			}

			offset = nextPageOffset;
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
