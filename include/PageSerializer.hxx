#ifndef PAGE_SERIALIZER_HXX
#define PAGE_SERIALIZER_HXX

#include <Configuration.hxx>
#include <RawDataUtils.hxx>

#include <vector>

template<Endianess endian>
class PageSerialize
{
	static std::vector<uint8_t> serialize(const DiskPage& page)
	{
		std::vector<uint8_t> result;

		result.push_back
	} 
};

#endif // PAGE_SERIALIZER_HXX
