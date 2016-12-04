#ifndef LRU_PAGE_REPLACE_POLICY_HXX
#define LRU_PAGE_REPLACE_POLICY_HXX

#include <Configuration.hxx>
#include <Optional.hxx>
#include <PageReplacePolicy.hxx>

#include <gsl/gsl_assert.h>

#include <vector>

template<Endianness endian>
class LRUPageReplacePolicy : public PageReplacePolicy<endian>
{
	public:
	using PageIndex = typename PageReplacePolicy<endian>::PageIndex;

	LRUPageReplacePolicy() = default;

	LRUPageReplacePolicy(size_type totalPageCount) noexcept
	: candidateQueue_{},
	  queuePagePosition_{totalPageCount}
	{}

	LRUPageReplacePolicy(const LRUPageReplacePolicy& other) = default;
	LRUPageReplacePolicy(LRUPageReplacePolicy&& other) = default;

	LRUPageReplacePolicy& operator=(const LRUPageReplacePolicy& other) = default;
	LRUPageReplacePolicy& operator=(LRUPageReplacePolicy&& other) = default;

	virtual void use(const DiskPage<endian>& page) override
	{
		auto pageId = page.getIndex();

		if(pageId >= queuePagePosition_.size())
		{
			queuePagePosition_.resize(pageId + 1);
		}
		if(queuePagePosition_[pageId])
		{
			auto position = queuePagePosition_[pageId];
			if(position)
			{
				candidateQueue_.erase(candidateQueue_.begin() + *position);
				queuePagePosition_[pageId] = {};
			}
		}
	}

	virtual void release(const DiskPage<endian>& page) override
	{
		auto pageId = page.getIndex();

		/* If our page is indexed, we did something wrong */
		Ensures(!queuePagePosition_[pageId]);

		candidateQueue_.push_back(DiskPageView{pageId, page.getSchemaName()});
		queuePagePosition_[pageId] = candidateQueue_.size() + 1;
	}

	virtual optional<PageIndex> getCandidate(const std::string& schemaName) override
	{
		/*PageIndex candidate = candidateQueue_.front();
		candidateQueue_.erase(candidateQueue_.begin());

		return candidate;*/
		for(auto pageView : candidateQueue_)
		{
			if(*pageView.schemaName == schemaName)
			{
				auto pageId = pageView.index;
				candidateQueue_.erase(candidateQueue_.begin() + *queuePagePosition_[pageId]);

				for(auto& pos : queuePagePosition_)
				{
					if(pos && (*pos > queuePagePosition_[pageId])) pos = pos - 1;
				}

				queuePagePosition_[pageId] = {};

				return pageId;
			}
		}
		return {};
	}

	private:

	struct DiskPageView
	{
		DiskPageView(PageIndex index_, const std::string& schemaName_)
		: index{index_},
		  schemaName{&schemaName_}
		{}

		DiskPageView(const DiskPageView&) = default;
		DiskPageView(DiskPageView&&) = default;
		DiskPageView& operator=(const DiskPageView&) = default;
		DiskPageView& operator=(DiskPageView&&) = default;

		PageIndex index;
		const std::string* schemaName;
	};

	/* Two vectors working together a bit like a sparse integer set */
	std::vector<DiskPageView> candidateQueue_;	
	std::vector<optional<size_type>> queuePagePosition_;
};

#endif // LRU_PAGE_REPLACE_POLICY_HXX
