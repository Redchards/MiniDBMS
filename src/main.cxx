#include <array>
#include <bitset>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <vector>

#include <iostream>
#include <chrono>
#include <Platform.hxx>
#include <ImprovedEnum.hxx>
#include <DataTypes.hxx>
#include <RawDataUtils.hxx>
#include <DbSchemaSerializer.hxx>
#include <FileValueReader.hxx>
#include <FileValueWriter.hxx>
#include <DbEntry.hxx>
#include <tuple>
#include <DiskPage.hxx>
#include <PageSerializer.hxx>
#include <PageWriter.hxx>
#include <PageReader.hxx>

static constexpr ConstString exprBegin = "const char *CTTI::GetTypeName() [T = ";
static constexpr ConstString exprEnd = "] ";

namespace CTTI
{    
    template<class T>
    static const char* GetTypeName()
    {
        static constexpr size_t typeNameLength = sizeof(FUNCTION) - (exprBegin.size() + exprEnd.size());
        static constexpr ConstString functionName = FUNCTION;
        static StaticString<typeNameLength + 1> typeName(functionName.begin() + exprBegin.size(), functionName.begin() + exprBegin.size() + typeNameLength);
        
        return typeName;
    }  
};

/*template<class Fn, class ... Args>
constexpr Fn for_each_args(Fn f, Args&& ... args)
{
    using expander = std::initializer_list<int>;
    return ((void)expander{(std::ref(f)(args), 0)...}, f);
}

template<class ... Args>
constexpr std::initializer_list<typename std::common_type<Args...>::type> fun(Args ... args)
{
    return {(..., args)};
}

template<class ReturnType, class ... Args>
constexpr size_t function_arity(ReturnType(*)(Args...))
{
    return sizeof...(Args);
}

enum myTstNum : int{ Foo, Bar, Bar2};
class leTst
{
    public:
    leTst(myTstNum){};
    template<class T>
    leTst operator=(T){ return *this; }  
};


#define ANOT_TST() mystr ledef[]{MAP((mystr), Foo, Bar, Baz, Der)}
void fooBar(leTst)
{}

//#define STRINGIFY(x) #x
//#define EXPAND(...) __VA_ARGS__

template<class T>
constexpr T leFouFou(T x)
{
    return x;
}

#define A() 123*/

template<class T>
constexpr T leFouFou(T x)
{
    return x;
}

#define F(x) std::cout << #x << std::endl; F(x);

IMPROVED_ENUM(mystr, int, Foo, Bar   =6, Baz, Der, Fist = 12);


IMPROVED_ENUM(Toto, int, 	
    Voiture = 0,
	Velo = 8,
	Trotinette
);

enum class Toto2 : uint16_t
{
    Voiture = 0,
    Velo = 8,
    Trotinette  
};


//BETTER_ENUM(mystr2, int, Foo, Bar=6, Dij=7, Baz=6);
//BETTER_ENUM(mystr2, int, Foo, Bar   =6, Baz, Der);

//    int tstdd = EXPAND(DEFER_EVAL(A)());

int main()
{
    std::vector<uint8_t> vevec{16, 0};
	std::vector<uint8_t> dateVec{16, 2, 7, 224};
	Utils::RawDataAdaptator<size_type, sizeof(size_type), Endianness::little> tt2{4};
	std::cout << (int)tt2.bytes[0] << std::endl;
    
    std::cout << Utils::RawDataConverter<Endianness::big>::rawDataToInteger(vevec.begin(), vevec.end()) << std::endl;
    std::cout << Utils::RawDataConverter<Endianness::little>::rawDataToInteger(vevec.begin(), vevec.end()) << std::endl;
    
    Utils::rawDataSwitchEndianness(vevec);
    for(auto elem : vevec)
    {
        std::cout << (int)elem << std::endl;
    }
	std::cout << Utils::RawDataStringizer<Endianness::big>::stringize(dateVec.begin(), dateVec.end(), DataTypeDescriptor{DataType::DATE}) << std::endl;
	
	std::vector<DbSchema> schList;
	static constexpr Endianness usedEndianness = Endianness::little;
    
    schList.push_back({"Book", {
        {"Title",  {DataType::CHARACTER, 10}},
        {"Editor", {DataType::CHARACTER, 15}},
		{"Parution", {DataType::DATE}}
    }});
	schList.push_back({"Employee", {
		{"Name", {DataType::CHARACTER, 25}},
		{"Surname", {DataType::CHARACTER, 25}},
		{"HiringDate", {DataType::DATE}}
	}});
	schList.push_back({"Runner", {
		{"Name", {DataType::CHARACTER, 25}},
		{"Surname", {DataType::CHARACTER, 25}},
		{"BestTime", {DataType::FLOAT, 24}},
		{"Number", {DataType::INTEGER}}
	}});
    DataType tt = DataType::DATE;
	std::cout << tt.toString() << std::endl;
	FileValueWriter<usedEndianness> writer{"db.sch"};
	for(auto& schema : schList)
	{
    	auto serialData = DbSchemaSerializer<usedEndianness>::serialize(schema);
		writer.write(serialData);
	}
	writer.unloadFile();
	FileValueReader<usedEndianness> reader{"db.sch"};
	while(!reader.eof()) {
		std::vector<uint8_t> serialData;
		auto readed = reader.readValue(8);
		serialData.resize(readed);
		std::cout << "readed : " << readed  << " : " << serialData.size() << std::endl;
		reader.read(serialData, readed);
    	std::cout << "Serialized" << std::endl;
		for(auto elem : serialData){ std::cout << (int)elem << " "; }
		for(auto elem : serialData){ std::cout << elem << "- "; }
		std::cout << std::endl;
    	std::cout << "Size : " << serialData.size() << std::endl;
    	auto schema2 = DbSchemaSerializer<usedEndianness>::deserialize(serialData.begin(), serialData.end());
 	    for(size_type i = 0; i < schema2.getFieldCount(); ++i)
		{
        	std::cout << schema2[i].name << " : " << schema2[i].type.toString() <<  std::endl;
    	}
		std::cout << std::endl;
	}
	auto wereEof = reader.eof();
	std::cout << std::boolalpha;
	std::cout << "Is end of file ? " << wereEof << std::endl;	

	std::vector<char> vecTst{'E', 'l', 'r', 'i', 'c', '\0', '\0', '\0', '\0', '\0'};
	std::string blip = "Omnibuslebossei";
	vecTst.insert(vecTst.end(), blip.begin(), blip.end());
	vecTst.push_back(16);vecTst.push_back(2);vecTst.push_back(7);vecTst.push_back(224);
	for(auto byte : vecTst){std::cout << byte << " ";}
	std::cout << DbEntry<Endianness::big>{schList[0], vecTst}.toString() << std::endl;

	bool end = false;
	auto schemaUsed = schList[2];

	std::vector<uint8_t> dummyDiskData{
		3, 4, 0, 0, 0, 0, 0, 0,
		0xa7, 0, 0, 0, 0, 0, 0, 0,
		43, 0, 0, 0,
        2, 0, 0, 0, 0, 0, 0, 0,
		'R', 'u', 'n', 'n', 'e', 'r', '\0',
		2, 0, 0, 0, 0, 0, 0, 0,
		0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
	};		
		
	DiskPage<usedEndianness> h21{0, dummyDiskData};
	DiskPage<usedEndianness> h{0, schemaUsed, 50};
	auto h2 = PageSerializer<usedEndianness>::serializeHeader(h);

	for(auto df : h2)
	{
		std::cout << (int)df << " ";
	}
	std::cout << std::endl;

	std::cout << "Next offset : " << h.getNextPageOffset() << std::endl;
	std::cout << "Page size : " << h.getPageSize() << std::endl;
	std::cout << "Schema name : " << h.getSchemaName() << std::endl;
	std::cout << "Is Full ? " << h.isFull() << std::endl;
	std::cout << "Raw page size : " << h.getRawPageSize() << std::endl;
	std::cout << "Header size : " << h.getHeaderSize() << std::endl;
	std::cout << "Data suze : " << h.getData().size() << std::endl;

	while(!end)
	{
		char ans;

		std::cout << "Voulez vous :\n";
		std::cout << "- a : ajouter une entrée\n";
		std::cout << "- b : supprimer une entrée\n";
		std::cout << "- c : modifier une entrée\n";
		std::cout << "- q : quitter\n";
		std::cin >> ans;

		if(ans == 'a')
		{
			std::vector<uint8_t> data;
			for(size_type i = 0; i < schemaUsed.getFieldCount(); ++i)
			{
				auto field = schemaUsed[i];
				size_t usedBytes = 0;
				std::cout << "Valeur pour " << field.name << "(" << field.type.toString() << ") : ";
				if(field.type.getType() == DataType::INTEGER)
				{
					size_type t;
					std::cin >> t;
					Utils::RawDataAdaptator<size_type, sizeof(size_type), usedEndianness> adapt{t};
					data.insert(data.end(), adapt.bytes.begin(), adapt.bytes.end());
					usedBytes = field.type.getSize();
				}
				else if((field.type.getType() == DataType::FLOAT) && (field.type.getSize() <= sizeof(float)))
				{
					float t;
					std::cin >> t;
					Utils::RawDataAdaptator<float, sizeof(float), Endianness::little> adapt{t};
					data.insert(data.end(), adapt.bytes.begin(), adapt.bytes.end());
					usedBytes = field.type.getSize();
				}

				else if((field.type.getType() == DataType::FLOAT) && (field.type.getSize() <= sizeof(double)))
				{
					double t;
					std::cin >> t;
					Utils::RawDataAdaptator<double, sizeof(double), Endianness::little> adapt{t};
					data.insert(data.end(), adapt.bytes.begin(), adapt.bytes.end());
					usedBytes = field.type.getSize();
				}
				else if(field.type.getType() == DataType::CHARACTER)
				{
					std::string str;
					std::cin >> str;
					data.insert(data.end(), str.begin(), str.end());
					usedBytes = str.size();
				}

				if(usedBytes < field.type.getSize())
				{
					std::cout << "resize with " << (field.type.getSize() - usedBytes) << " bytes" << std::endl;
					data.resize(data.size() + field.type.getSize() - usedBytes);
				} 
			}

			for(auto d : data)
			{
				std::cout << (int)d << " ";
			}
			DbEntry<usedEndianness> newEntry{schemaUsed, data};
			std::cout << std::boolalpha << h.add(newEntry) << std::endl;
			std::cout << newEntry.toString() << std::endl;
		}
		else if(ans == 'q')
		{
			end = true;
		}
	}

	PageWriter<usedEndianness> pgWriter("db");
	pgWriter.writePage(h, 0);

	PageReader<usedEndianness> pgReader("db");
	auto rdPg = pgReader.readPage(0, 0);

	// DiskPage<usedEndianness> rdPg{0, h.getData()};

	for(int i = 0; i < 4; ++i)
	{
		std::vector<uint8_t> vTest{rdPg.getData().begin() + i*schemaUsed.getDataSize(), rdPg.getData().begin() + (i+1)*schemaUsed.getDataSize()};
			for(auto d : vTest)
			{
				std::cout << (int)d << " ";
			}
		std::cout << std::endl << std::endl;
		auto h3 = PageSerializer<usedEndianness>::serialize(h);
		for(auto d : h3)
	{std::cout << (int)d << " ";}
		DbEntry<usedEndianness> ent{schemaUsed, vTest};
		std::cout << ent.toString() << std::endl;
	}
	
	std::cout << PageSerializer<usedEndianness>::serialize(h).size()<< std::endl;

    
    return 0;
}