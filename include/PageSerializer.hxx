#ifndef PAGE_SERIALIZER_HXX
#define PAGE_SERIALIZER_HXX

#include <Configuration.hxx>
#include <RawDataUtils.hxx>

#include <vector>

template<Endianness endian>
class PageSerializer
{
	public:
	static std::vector<uint8_t> serializeHeader(const DiskPage<endian>& page) noexcept
	{
		// Should reserve something
		std::vector<uint8_t> result;

		using offsetType = decltype(page.getNextPageOffset());
		Utils::RawDataAdaptator<offsetType, sizeof(offsetType), endian> offsetData{page.getNextPageOffset()};
		result.insert(result.end(), offsetData.bytes.begin(), offsetData.bytes.end());

		using pageSizeType = decltype(page.getPageSize());
		Utils::RawDataAdaptator<pageSizeType, sizeof(pageSizeType), endian> pageSizeData{page.getPageSize()};
		result.insert(result.end(), pageSizeData.bytes.begin(), pageSizeData.bytes.end());

		// We can only used the getter and not copy, because the getter is giving back a string view.
		result.insert(result.end(), page.getSchemaName().begin(), page.getSchemaName().end());

		using freeSlotCountType = decltype(page.getFreeSlotCount());
		Utils::RawDataAdaptator<freeSlotCountType, sizeof(freeSlotCountType), endian> freeSlotCountData{page.getFreeSlotCount()};
		result.insert(result.end(), freeSlotCountData.bytes.begin(), freeSlotCountData.bytes.end());

		result.insert(result.end(), page.getFrameIndicators().begin(), page.getFrameIndicators().end());

		return result;
	}

	static std::vector<uint8_t> serialize(const DiskPage<endian>& page) noexcept
	{
		std::vector<uint8_t> result{serializeHeader(page)};
		result.insert(result.end(), page.getData().begin(), page.getData().end());

		return result;
	}
};

#endif // PAGE_SERIALIZER_HXX
