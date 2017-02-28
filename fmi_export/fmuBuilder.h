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
#ifndef _FMI_BUILDER_H_
#define _FMI_BUILDER_H_

#include <memory>
#include <vector>
#include <string>

class fmiCoordinator;

//class gridDynSimulation;
/** object to build an FMI object from a gridDyn simulation file*/
class fmuBuilder
{
private:
	const std::string fmuLoc;
	std::vector<unsigned int> vrs;
	std::shared_ptr<fmiCoordinator> coord;
public:

	fmuBuilder();
	fmuBuilder(const std::string &fmuLocation);

	void MakeFmu(const std::string &fmuLocation="");
	friend class fmiCoordinator;
};
#endif
