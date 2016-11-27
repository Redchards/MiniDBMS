#ifndef BUFFER_MANAGER_HXX
#define BUFFER_MANAGER_HXX

#include <Configuration.hxx>
#include <RawDataUtils.hxx>
#include <DataTypes.hxx>
#include <DiskPage.hxx>
#include <LRUPageReplacePolicy.hxx>
#include <Meta.hxx>
#include <PageReader.hxx>
#include <PageWriter.hxx>

#include <mutex>
#include <thread>
#include <vector>

enum class PageType : uint8_t
{
	ReadOnly,
	Writable
};

class BufferManager
{
	static constexpr size_type defaultBufferSize = 4096;
	static constexpr size_type pageSize = 512;

	public:
	using PageIndex = typename DiskPage<endian>::PageIndex
	using DefaultPolicy = LRUPageReplacePolicy;

	public:
	BufferManager(const std::string& dbFileName)
	: bufferPool_{defaultBufferSize},
	  pinCountList_{},
	  replacePolicy_{new DefaultPolicy()},
	  bufferPagePosition_{},
	  pgReader_{dbFileName},
	  pgWriter_{dbFileName}
	{}

	BufferManager(const std::string& dbFileName, size_type bufferSize)
	: bufferPool_{bufferSize},
	  pinCountList_{},
	  replacePolicy_{new DefaultPolicy()},
	  bufferPagePosition_{},
	  pgReader_{dbFileName},
	  pgWriter_{dbFileName}
	{}

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
			pinCountList_[pageId].fetch_add(1);
			replacePolicy_->use(pageId);
			/*}*/
		}
	}

	void unpin(PageIndex pageId) noexcept
	{
		//std::lock_guard<std::mutex> lock{mut_};
		size_type pinCount = pinCountList[pageId]; 
		if((pageId < bufferPool_.size()) && (pinCount > 0))
		{
			pinCountList_[pageId] = pinCount - 1;
		}
		if(pinCount == 0)
		{
			replacePolicy_->release(pageId);
		}
	}

	template<PageType type>
	DiskPage& requestPage(const DbSchema& schema)
	{
		auto candidatePageIt = firstAvailablePageOffset_.find(schema.getName());

		if(candidatePageIt != firstAvailablePageOffset_.end())
		{
			auto pos = bufferPagePosition_.find(*candidatePageIt);
			
			if(pos != bufferPagePosition_.end())
			{
				return bufferPool_[*pos];
			}
			else
			{
				performPageReplace(schema.getName());
				return requestPage(schema.getName());
			}
		}
		else
		{
			bufferPool_
		}
	}

	/*template<>
	const DiskPage& requestPage<PageType::NonModifiable>(const std::string& schemaName)
	{
		return requestPageAux(schemaName);
	}*/

	private:

	void performPageReplace(const std::string& schemaName)
	{
		auto candidatePageId = replacePolicy_->getCandidate(schemaName);
		pgReader
	}

	std::vector<DiskPage> bufferPool_;
	std::vector<size_type> pinCountList_;
	std::unique_ptr<PageReplacePolicy> replacePolicy_;
	std::unordered_map<std::streamoff, size_type> bufferPagePosition_;

	/* It is guaranteed that the page at the "std::streamoff" will not be full.
	 * However, they may be no such page, in which case firstAvailablePageOffset_[schemaName] will return the "end" iterator */
	std::unordered_map<std::string, std::streamoff> firstAvailablePageOffset_;

	PageReader<endian> pgReader_;
	PageWriter<endian> pgWriter_;
};

#endif // BUFFER_MANAGER_HXX
