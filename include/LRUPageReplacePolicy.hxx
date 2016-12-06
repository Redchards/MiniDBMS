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

		// Ensures(!queuePagePosition_[pageId]);

		candidateQueue_.push_back(pageId);
		queuePagePosition_[pageId] = candidateQueue_.size() - 1;
	}

	virtual optional<PageIndex> getCandidate() override
	{
		/*PageIndex candidate = candidateQueue_.front();
		candidateQueue_.erase(candidateQueue_.begin());

		return candidate;*/
			std::cout << "search candidate" << std::endl;
		for(auto pageId : candidateQueue_)
		{
			candidateQueue_.erase(candidateQueue_.begin() + *queuePagePosition_[pageId]);

			for(auto& pos : queuePagePosition_)
			{
				if(pos && (*pos > queuePagePosition_[pageId])) pos = pos - 1;
			}

			queuePagePosition_[pageId] = {};

			return pageId;
		}

		return {};
	}

	private:

	/* Two vectors working together a bit like a sparse integer set */
	std::vector<PageIndex> candidateQueue_;	
	std::vector<optional<size_type>> queuePagePosition_;
};

#endif // LRU_PAGE_REPLACE_POLICY_HXX
