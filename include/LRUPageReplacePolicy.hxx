#ifndef LRU_PAGE_REPLACE_POLICY_HXX
#define LRU_PAGE_REPLACE_POLICY_HXX

#include <Configuration.hxx>
#include <Optional.hxx>
#include <PageReplacePolicy.hxx>

#include <gsl/gsl_assert.hx>

#include <vector>

class LRUPageReplacePolicy : public PageReplacePolicy
{
	public:
	using PageIndex = PageReplacePolicy::PageIndex;

	LRUPageReplacePolicy() = default;

	LURPageReplacePolicy(size_type totalPageCount)
	: candidateQueue_{},
	  queuePagePosition_{totalPageCount}

	virtual void use(const DiskPage& page) override
	{
		auto pageId = page.getIndex();

		if(pageId >= queuePagePosition_.size())
		{
			queuePagePosition_.resize(pageId);
		}
		if(queuePagePosition[pageId])
		{
			candidateQueue_.erase(candidateQueue_.begin() + *queuePagePosition[pageId]);
			*queuePagePosition[pageId] = {};
		}
	}

	virtual void release(const DiskPage& page) override
	{
		auto pageId = page.getIndex();

		/* If our page is indexed, we did something wrong */
		Ensures(!queuePagePosition[pageId]);

		candidateQueue_.push_back({pageId, page.getSchemaName()});
		queuePagePosition_[pageId] = candidateQueue_.size() + 1;
	}

	virtual optional<PageIndex> getCandidate(const std::string& schemaName) override
	{
		/*PageIndex candidate = candidateQueue_.front();
		candidateQueue_.erase(candidateQueue_.begin());

		return candidate;*/
		for(auto pageView : candidateQueue_)
		{
			if(pageView.schemaName == schemaName)
			{
				auto pageId = pageView.index;
				candidateQueue_.erase(candidateQueue_.begin() + *queuePagePosition_[pageId]);

				for(auto& pos : queuePagePosition_)
				{
					if(pos && (*pos > queuePagePosition_[pageId])) pos = pos - 1;
				}

				queuePagePosition_[pageId] = {}

				return pageId
			}
		}
		return {};
	}

	private:

	struct DiskPageView
	{
		BufferPageView(size_type index_, const std::string& schemaName_)
		: index{index_},
		  schemaName{schemaName_}
		{}

		size_type index;
		const std::string& schemaName;
	}

	/* Two vectors working together a bit like a sparse integer set */
	std::vector<DiskPageView> candidateQueue_;	
	std::vector<optional<size_type>> queuePagePosition_;
};

#endif // LRU_PAGE_REPLACE_POLICY_HXX
