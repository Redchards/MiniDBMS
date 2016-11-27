#ifndef PAGE_REPLACE_POLICY_HXX
#define PAGE_REPLACE_POLICY_HXX

#include <DiskPage.hxx>
#include <Optional.hxx>

#include <string>

class PageReplacePolicy
{
	public
	using PageIndex = typename DiskPage<endian>::PageIndex

	public:
	virtual void use(const DiskPage& page) = 0;
	virtual void release(const DiskPage& page) = 0;
	virtual optional<PageIndex> getCandidate(const std::string& schemaName) = 0;
};

#endif // PAGE_REPLACE_POLICY_HXX
