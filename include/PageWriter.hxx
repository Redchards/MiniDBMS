#ifndef PAGE_WRITER_HXX
#define PAGE_WRITER_HXX

#include <Configuration.hxx>
#include <DiskPage.hxx>
#include <FileStream.hxx>
#include <PageSerializer.hxx>

#include <string>

template<Endianness endian>
class PageWriter : public FileStreamBase<StreamGoal::write>
{
	using Base = FileStreamBase<StreamGoal::write>;

	public:
	PageWriter(const std::string& fileName)
	: Base(fileName, std::ios_base::out | std::ios_base::in | std::ios::binary)
	{}

	void writePage(const DiskPage<endian>& page, std::streampos pos)
	{
		this->write(PageSerializer<endian>::serialize(page), pos);
		this->flush();
	}

	void appendPage(const DiskPage<endian>& page)
	{
		this->append(PageSerializer<endian>::serialize(page));
		this->flush();
	}

	void writePageHeader(const DiskPageHeader<endian>& header, std::streampos pos)
	{
		this->write(PageSerializer<endian>::serializeHeader(header), pos);
		this->flush();
	}
};

#endif // PAGE_WRITER_HXX
