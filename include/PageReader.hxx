#ifndef PAGE_READER_HXX
#define PAGE_READER_HXX

#include <Configuration.hxx>
#include <DiskPage.hxx>
#include <FileValueReader.hxx>

#include <string>
#include <vector>

template<Endianness endian>
class PageReader : protected FileValueReader<endian>
{
	using Base = FileValueReader<endian>;
	using PageIndex = typename DiskPage<endian>::PageIndex;

	public:
	PageReader(const std::string& fileName)
	: Base(fileName, std::ios_base::in | std::ios::binary)
	{}

	auto readRawPageSize(std::streampos pos)
	{
		static constexpr size_type offset = sizeof(decltype(std::declval<DiskPageHeader<endian>>().getNextPageOffset()));

		return this->readValue(sizeof(decltype(std::declval<DiskPageHeader<endian>>().getRawPageSize())), static_cast<std::streamoff>(pos) + offset);
	}

	auto readHeaderSize(std::streampos pos)
	{
		static constexpr size_type offset = sizeof(decltype(std::declval<DiskPageHeader<endian>>().getNextPageOffset())) + sizeof(decltype(std::declval<DiskPageHeader<endian>>().getRawPageSize()));

		return this->readValue(sizeof(decltype(std::declval<DiskPageHeader<endian>>().getHeaderSize())), static_cast<std::streamoff>(pos) + offset);
	}

	DiskPage<endian> readPage(PageIndex index, std::streampos pos)
	{
	std::cout << "RAAAAA : " << std::endl;
		const auto rawPageSize = readRawPageSize(pos);
		std::cout << "RAAAAA : " <<rawPageSize << std::endl;
		std::vector<uint8_t> data(rawPageSize);
		this->read(data, rawPageSize, pos);
		for(auto d : data)
	{std::cout << (int)d << " ";}

		return {index, std::move(data)};
	}

	DiskPageHeader<endian> readPageHeader(std::streampos pos)
	{
		const auto headerSize = readHeaderSize(pos);
		std::vector<uint8_t> data(headerSize);
		this->read(data, headerSize, pos);

		return {std::move(data)};
	}
};

#endif // PAGE_READER_HXX
