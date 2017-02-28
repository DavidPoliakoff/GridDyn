/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil;  eval: (c-set-offset 'innamespace 0); -*- */
/*
* LLNS Copyright Start
* Copyright (c) 2017, Lawrence Livermore National Security
* This work was performed under the auspices of the U.S. Department
* of Energy by Lawrence Livermore National Laboratory in part under
* Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
* Produced at the Lawrence Livermore National Laboratory.
* All rights reserved.
* For details, see the LICENSE file.
* LLNS Copyright End
*/
#pragma once
#ifndef GENERIC_STRING_OPS_H_
#define GENERIC_STRING_OPS_H_

/** this file defines some operations that can be performed on string like objects
*/
#include <vector>
#include "charMapper.h"

template<class X>
std::vector<X> generalized_string_split(const X&str, const X& delimiterCharacters, bool compress)
{
	std::vector<X> ret;

	
	auto pos = str.find_first_of(delimiterCharacters);
	decltype(pos) start = 0;
	while (pos != X::npos)
	{
		if (pos != start)
		{
			ret.push_back(str.substr(start, pos - start));
		}
		else if (!compress)
		{
			ret.push_back(X());
		}
		start = pos + 1;
		pos = str.find_first_of(delimiterCharacters, start);
	}
	if (start < str.length())
	{
		ret.push_back(str.substr(start));
	}
	else if (!compress)
	{
		ret.push_back(X());
	}
	return ret;
}

template<class X>
std::vector<X> generalized_section_splitting(const X& line, const X& delimiterCharacters, const X& sectionStartCharacters, const charMapper<unsigned char> &sectionMatch, bool compress)
{
	auto sectionLoc = line.find_first_of(sectionStartCharacters);

	if (sectionLoc == X::npos)
	{
		return generalized_string_split(line, delimiterCharacters, compress);
	}

	auto d1 = line.find_first_of(delimiterCharacters);
	if (d1 == X::npos) //there are no delimiters
	{
		return{ line };
	}
	decltype(sectionLoc) start = 0;
	std::vector<X> strVec;
	while (start < line.length())
	{
		if (sectionLoc > d1)
		{
			if (start == d1)
			{
				if (!compress)
				{
					strVec.push_back(X());
				}
			}
			else
			{
				strVec.push_back(line.substr(start, d1 - start));
			}
			start = d1 + 1;
			d1 = line.find_first_of(delimiterCharacters, start);
		}
		else //now we are in a quote
		{
			sectionLoc = line.find_first_of(sectionMatch[line[sectionLoc]], sectionLoc + 1);
			if (sectionLoc != X::npos)
			{
				d1 = line.find_first_of(delimiterCharacters, sectionLoc + 1);
				sectionLoc = line.find_first_of(sectionStartCharacters, d1 + 1);
			}
			else
			{
				strVec.push_back(line.substr(start));
				start = sectionLoc;
			}

		}
		//get the last string
		if (d1 == X::npos)
		{
			if ((start < line.length()) || (!compress))
			{
				strVec.push_back(line.substr(start));
			}
			start = d1;
		}

	}
	return strVec;
}

#endif