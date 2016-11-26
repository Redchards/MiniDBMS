#ifndef BUFFER_MANAGER_HXX
#define BUFFER_MANAGER_HXX

#include <Configuration.hxx>
#include <RawDataUtils.hxx>
#include <DataTypes.hxx>
#include <DiskPage.hxx>

#include <bitset>
#include <queue>
#include <vector>

class BufferManager
{
	static constexpr size_t defaultBufferSize = 4096;
	static constexpr size_t pageSize = 512;

	using PageIndex = typename DiskPage<endian>::PageIndex

	public:
	BufferManager()
	: candidateQueue_{},
	  bufferPool_{defaultBufferSize}
	{}

	BufferManager(size_t bufferSize)
	: candidatQueue_{},
	  bufferPool_{bufferSize}
	{}

	private:

	std::queue<PageIndex> candidateQueue_;
	std::vector<DiskPage> bufferPool_;
};

#endif // BUFFER_MANAGER_HXX
