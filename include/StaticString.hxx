#ifndef STATIC_STRING_HXX
#define STATIC_STRING_HXX

#include <ArrayIteratorPolicy.hxx>
#include <ConstexprAssert.hxx>
#include <ConstString.hxx>
#include <Range.hxx>
#include <StringDetails.hxx>


template<size_t Tsize>
class StaticString
{
	using IterPolicy = ArrayIteratorPolicy<StaticString<Tsize>>;
	
public:
	using iterator = typename IterPolicy::iterator;
	using reverse_iterator = typename IterPolicy::reverse_iterator;
	using const_iterator = typename IterPolicy::const_iterator;
	using const_reverse_iterator = typename IterPolicy::const_reverse_iterator;
	using unchecked_iterator = typename IterPolicy::unchecked_iterator;
	using unchecked_const_iterator = typename IterPolicy::unchecked_const_iterator;
	using unchecked_reverse_iterator = typename IterPolicy::unchecked_reverse_iterator;
	using unchecked_const_reverse_iterator = typename IterPolicy::unchecked_const_reverse_iterator;
	
	using value_type = char;
	using reference = char&;
	using pointer = char*;
	
public:
	// Serve for the sole purpose of begin able to be literal type even with default constructor
	constexpr StaticString() : str_{}, actualSize_{}
	{}
	constexpr StaticString(const StaticString& other) : str_{other.rawData()}, actualSize_{other.size()}
	{
		//static_assert(otherSize <= Tsize, "The string used to initialize the StaticString do not fit !");
	}
	
	template<size_t otherSize>
	constexpr StaticString(const StaticString<otherSize>& other) : str_{other.rawData()}, actualSize_{other.size()}
	{
		static_assert(otherSize <= Tsize, "The string used to initialize the StaticString do not fit !");
	}
	
	template<size_t otherSize>
	constexpr StaticString(const char(&cstr)[otherSize]) : str_{initStr<otherSize - 1>(ConstString{cstr})}, actualSize_{otherSize - 1}
	{
		static_assert(otherSize <= (Tsize + 1), "The string used to initialize the StaticString do not fit !");
	}

	template<class Iterator>
	constexpr StaticString(Iterator begin, Iterator end) : StaticString{range<Iterator>{begin, end}}
	{}
	
	template<class Iterator>
	constexpr StaticString(range<Iterator> range) : str_{initStrRange<Iterator, Tsize>(range)}, actualSize_{range.size()}
	{
		CONSTEXPR_ASSERT(range.size() <= Tsize, "Range do not fit in the StaticString !");
	}
	
	// Slow version to compile on libc++ which seems buggy on my archlinux.
	/*template<class TString>
	constexpr StaticString(TString&& str) : StaticString{range<typename TString::const_iterator>{str.begin(), str.end()}}
	{
		//CONSTEXPR_ASSERT(str.size() <= Tsize, "The string used to initialize the StaticString do not fit !");
	}*/
	
	constexpr iterator begin() noexcept { return{ *this, 0 }; }
	constexpr const_iterator begin() const noexcept { return{ *this, 0 }; }
	constexpr const_iterator cbegin() const noexcept { return begin(); }
	constexpr iterator end() noexcept { return{ *this, size() }; }
	constexpr const_iterator end() const noexcept { return{ *this, size() }; }
	constexpr const_iterator cend() const noexcept { return end(); }

	constexpr reverse_iterator rbegin() noexcept { return end(); }
	constexpr const_reverse_iterator rbegin() const noexcept { return cend(); }
	constexpr const_reverse_iterator crbegin() const noexcept { return cend(); }
	constexpr reverse_iterator rend() noexcept { return begin(); }
	constexpr const_reverse_iterator rend() const noexcept { return cbegin(); }
	constexpr const_reverse_iterator crend() const noexcept { return cbegin(); }

	constexpr char& at(size_t index)
	{
		CONSTEXPR_ASSERT(index < actualSize_, "Attempt to access a non-existing index of a StaticString");
		return str_[index];
		//return (index < actualSize_ ? str_[index] : throw std::out_of_range("Attempt to access a non-existing index of a StaticString"));
	}
	
	constexpr const char& at(size_t index) const
	{
		CONSTEXPR_ASSERT(index < actualSize_, "Attempt to access a non-existing index of a StaticString");
		return str_[index];
	}
	
	constexpr char& operator[](size_t index)
	{
		return at(index);
		//return (index < actualSize_ ? str_[index] : throw std::out_of_range("Attempt to access a non-existing index of a StaticString"));
	}

	constexpr const char& operator[](size_t index) const
	{
		return at(index);
		//return (index < actualSize_ ? str_[index] : throw std::out_of_range("Attempt to access a non-existing index of a StaticString"));
	}

	constexpr const char* data() const
	{
		return &str_[0];
	}
	
	constexpr char* data()
	{
		return &str_[0];
	}

	constexpr auto rawData() const
	{
		return str_;
	}
	
	constexpr void resize(size_t newSize) noexcept
	{
		CONSTEXPR_ASSERT(newSize <= Tsize, "Cannot resize a StaticString past it's maximum size");
		
		actualSize_ = newSize;
	}
	
	constexpr auto find(char c) noexcept
	{
		for(auto it = begin(); it != end(); ++it)
		{
			if(*it == c)
			{
				return it; 
			}
		}
		return end();	
	}
	
	constexpr auto find(char c) const noexcept
	{
		for(auto it = cbegin(); it != cend(); ++it)
		{
			if(*it == c)
			{
				return it; 
			}
		}
		return cend();
	}
	
	constexpr auto findLastOf(char c) noexcept
	{
		for(auto it = rbegin(); it != rend(); ++it)
		{
			if(*it == c)
			{
				return it;
			}
		}
		return rend();
	}
	
	constexpr auto findLastOf(char c) const noexcept
	{
		for(auto it = crbegin(); it != crend(); ++it)
		{
			if(*it == c)
			{
				return it;
			}
		}
		return crend();
	}
	
	constexpr StaticString trim() const noexcept
	{
		return StaticString{range<StaticString::const_iterator>{this->begin(), this->find(' ')}};
	}
	
	StaticString& trim() noexcept
	{
		for(auto it = rbegin(); it != rend(); ++it)
		{
			if(*it == ' ')
			{
				*it = '\0';
				--actualSize_;
			}
			else
			{ 
				return *this;
			}
		}
		return *this;
	}

	constexpr operator const char*() const noexcept
	{
		return data();
	}
	
	/*template<size_t TSize1, size_t TSize2>
	friend constexpr bool operator==(const StaticString<TSize1>& lhs, const StaticString<TSize2>& rhs);
	
	template<size_t TSize1, size_t TSize2>
	friend constexpr bool operator!=(const StaticString<TSize1>& lhs, const StaticString<TSize2>& rhs);
	
	template<size_t otherSize>
	friend constexpr bool operator==(const StaticString<otherSize>& lhs, ConstString rhs);
	
	template<size_t otherSize>
	friend constexpr bool operator==(ConstString lhs, const StaticString<otherSize>& rhs);*/
	
	/* Alternative definition, working on GCC 5.2.0 and Clang 3.6
	
	constexpr bool operator==(StaticString<Tsize> rhs) const
    {
    	for(uint8_t i = 0; i < size(); ++i)
    	{
    		if((*this)[i] != rhs[i])
    		{
    			return false;
    		}
    	}
    	return true;
    }
	*/
    
    /*template<size_t otherSize>
    constexpr bool operator==(StaticString<otherSize>) const
    {
    	return false;
    }*/

	constexpr size_t size() const noexcept { return actualSize_; }

private:
	/* 
	 * Using this helper class for initialization causes GCC to freak out and see 'non constant expression' everywhere.
	 * So not using it make up for a slightly uglier but working code on GCC
	 */
	/* template<class Iterator>
	class RangeInitializationHelper
	{
		public:
		constexpr RangeInitializationHelper(const range<Iterator>& range) noexcept : range_{range}
		{}
		constexpr char operator[](size_t index) const noexcept
		{
			return (range_.begin() + index) < range_.end() ? *(range_.begin() + index) : '\0';
		}
		
		private:
		const range<Iterator>& range_;	
	}; */

	template<size_t size>
	constexpr std::array<char, Tsize + 1> initStr(ConstString cstr) const noexcept
	{
		return initStrAux(cstr, std::make_integer_sequence<size_t, size>{});
	}
	
	template<class Iterator, size_t size>
	constexpr std::array<char, Tsize + 1> initStrRange(const range<Iterator>& range) const noexcept
	{
		//using IteratorType = std::conditional_t<Meta::is_iterator_of<StaticString, Iterator>::value, unchecked_const_iterator, Iterator>;
		return initStrRangeAux(range, std::make_integer_sequence<size_t, size>{});
	}
	
	template<size_t ... indices>
	constexpr std::array<char, Tsize + 1> initStrAux(ConstString cstr, std::integer_sequence<size_t, indices...>) const noexcept
	{
		return {{cstr[indices]...}};
	}
	
	template<class Iterator, size_t ... indices>
	constexpr std::array<char, Tsize + 1> initStrRangeAux(const range<Iterator>& range, std::integer_sequence<size_t, indices...>) const noexcept
	{
		return {{((range.begin() + indices) < range.end() ? *(range.begin() + indices) : '\0')...}};
	}

	std::array<char, Tsize + 1> str_;
	size_t actualSize_;
};

template<size_t TSize1, size_t TSize2>
constexpr bool operator==(StaticString<TSize1>& lhs, StaticString<TSize2> rhs)
{
	return lhs.size() != rhs.size() ? false : Details::equalAux(lhs.size(), lhs, rhs);
}

template<size_t TSize1, size_t TSize2>
constexpr bool operator!=(StaticString<TSize1>& lhs, StaticString<TSize2> rhs)
{
	return !(lhs == rhs);
}

template<size_t TSize1, size_t TSize2>
constexpr bool operator==(const StaticString<TSize1>& lhs, const char (&rhs)[TSize2])
{
	ConstString tmp{rhs};
	return lhs.size() != tmp.size() ? false : Details::equalAux(lhs.size(), lhs, tmp);
}
	
template<size_t TSize1, size_t TSize2>
constexpr bool operator==(const char (&lhs)[TSize1], const StaticString<TSize2>& rhs)
{
	return rhs == lhs;
}

template<size_t TSize1, size_t TSize2>
constexpr bool operator!=(const char (&lhs)[TSize1], const StaticString<TSize2>& rhs)
{
	return !(rhs == lhs);
}

template<size_t TSize1, size_t TSize2>
constexpr bool operator!=(const StaticString<TSize1>& lhs, const char (&rhs)[TSize2])
{
	return !(rhs == lhs);
}

template<size_t TSize>
constexpr bool operator==(const StaticString<TSize>& rhs, ConstString lhs)
{
	return lhs.size() != rhs.size() ? false : Details::equalAux(lhs.size(), lhs, rhs);
}
	
template<size_t TSize>
constexpr bool operator==(ConstString lhs, const StaticString<TSize>& rhs)
{
	return rhs == lhs;
}

template<size_t TSize>
constexpr bool operator!=(const StaticString<TSize>& lhs, ConstString rhs)
{
	return !(lhs == rhs);
}
	
template<size_t TSize>
constexpr bool operator!=(ConstString lhs, const StaticString<TSize>& rhs)
{
	return !(rhs == lhs);
}

#endif // STATIC_STRING_HXX
