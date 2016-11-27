#ifndef PAGE_WRITER_HXX
#define PAGE_WRITER_HXX

#include <Configuration.hxx>
#include <DiskPage.hxx>
#include <FileStream>

template<Endianess endian>
class PageWriter : protected FileStreamBase<endian>
{
	using Base = FileStreamBase<endian>;

	public:
	PageWriter(const std::string& fileName)
	: Base(fileName)
	{}

	void writePage(
};

#endif // PAGE_WRITER_HXX
