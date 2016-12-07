#ifndef QUERY_PARSER_HXX
#define QUERY_PARSER_HXX

#include <Configuration.hxx>
#include <DataType.hxx>
#include <DbSystem.hxx>

#include <exception>

class ParsingException : public std::exception
{
public:
	ParsingException(const std::string& symbol, const std::string& desc)
	: msg_{std::string{"Error during the query parsing : " + symbol + " (" + desc + ")"}
	{}

	virtual const char* what() const noexcept override
	{
		return msg.c_str();
	}

private:
	msg_;
};

template<Endianness endian>
class QueryIterator
{
public:
	QueryIterator(DbSystem<endian>& system)
	: iterator_{system.getIterator()},
	  endIterator_{
	{}

	DbEntry<endian, PageType::ReadOnly> operator*() noexcept
	{
		auto dataStart = pageHandle_.get()->getData().begin() + (currentEntryIndex_ * schema_.getDataSize());
		std::vector<uint8_t> tmp(dataStart, dataStart + schema_.getDataSize());
		return {schema_, tmp};
	}


protected:
	virtual DbEntry<endian, PageType::ReadOnly> next() noexcept
	{
		if(iterator_ != endIterator)
		{
			
		}
	}

	DbIterator<endian> iterator_;
	DbIterator<endian> endIterator_;
};

template<Endianness endian>
class Query
{
public:
	Query(DbSystem<endian>& system, 
		  const std::string& command, 
		  std::vector<std::pair<const DbSchema&>, std::string>&& columns,
		  DbSchema&& schema, 
		  std::function<bool(const DbEntry<endian, PageType::Writable>&)> pred)
	: system_{system},
	  command_{command},
	  columns{std::move(columns)},
	  schema_{std::move(schema)},
	  pred_{pred}
	{}

	DbIterator<endian> execute()
	{
	}

	void executeUpdate()
	{
	}

private:
	DbSystem<endian>& system_;
	const std::string command_;
	const std::vector<std::pair<const DbSchema&>, std::string> columns_;
	const DbSchema schema_;
	std::function<bool(const DbEntry<endian, PageType::Writable>&)> pred_;
};

template<Endianness endian>
class QueryParser
{
public:
	QueryParser(DbSystem<endian>& system)
	: system_{system}
	{}

private:
	DbSystem<endian>& system_;
};

#endif // QUERY_PARSER_HXX
