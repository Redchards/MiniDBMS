#ifndef PAGE_REPLACE_POLICY_HXX
#define PAGE_REPLACE_POLICY_HXX

#include <Configuration.hxx>
#include <DiskPage.hxx>
#include <Optional.hxx>
#include <BufferManager.hxx>

#include <string>

template<Endianness endian>
class PageReplacePolicy
{
	public:
	using PageIndex = typename DiskPage<endian>::PageIndex;

	public:
	virtual void use(const DiskPage<endian>& page) = 0;
	virtual void release(const DiskPage<endian>& page) = 0;
	virtual optional<PageIndex> getCandidate(const std::string& schemaName) = 0;
};

#endif // PAGE_REPLACE_POLICY_HXX
