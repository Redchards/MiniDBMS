#ifndef PAGE_WRITER_HXX
#define PAGE_WRITER_HXX

#include <Configuration.hxx>
#include <DiskPage.hxx>
#include <FileStream.hxx>
#include <PageSerializer.hxx>

template<Endianness endian>
class PageWriter : protected FileStreamBase<StreamGoal::write>
{
	using Base = FileStreamBase<StreamGoal::write>;

	public:
	PageWriter(const std::string& fileName)
	: Base(fileName, std::ios_base::out | std::ios::binary)
	{}

	void writePage(const DiskPage<endian>& page, std::streampos pos)
	{
		this->write(PageSerializer<endian>::serialize(page), pos);
	}
};

#endif // PAGE_WRITER_HXX
