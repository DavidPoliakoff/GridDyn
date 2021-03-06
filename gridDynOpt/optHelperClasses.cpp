/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil;  eval: (c-set-offset 'innamespace 0); -*- */
/*
   * LLNS Copyright Start
 * Copyright (c) 2016, Lawrence Livermore National Security
 * This work was performed under the auspices of the U.S. Department
 * of Energy by Lawrence Livermore National Laboratory in part under
 * Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
 * Produced at the Lawrence Livermore National Laboratory.
 * All rights reserved.
 * For details, see the LICENSE file.
 * LLNS Copyright End
 */

#include "optHelperClasses.h"
#include <cstring>


bool isAC (const optimMode &oMode)
{
  return (oMode.flowMode == flowModel_t::ac);
}


void optimSizes::reset ()
{
  std::memset (this, 0, sizeof(optimSizes));
}


void optimSizes::add (const optimSizes &arg)
{
  vSize += arg.vSize;
  aSize += arg.aSize;
  genSize += arg.genSize;
  qSize += arg.qSize;
  intSize += arg.intSize;
  contSize += arg.contSize;
  constraintsSize += arg.constraintsSize;
}

optimOffsets::optimOffsets (optimOffsets *no)
{
  std::memcpy (this,no,sizeof(optimOffsets));
}
optimOffsets::optimOffsets (const optimOffsets &nOffsets)
{
  std::memcpy (this, &nOffsets, sizeof(optimOffsets));
}

void optimOffsets::reset ()
{
  gOffset = qOffset = vOffset = aOffset = contOffset = intOffset = kNullLocation;
  constraintOffset = 0;
  total.reset ();
  local.reset ();

  loaded = false;
}

void optimOffsets::increment ()
{
  count_t contExtra = 0;
  if (aOffset != kNullLocation)
    {
      aOffset += total.aSize;
    }
  else
    {
      contExtra = total.aSize;
    }
  if (vOffset != kNullLocation)
    {
      vOffset += total.vSize;
    }
  else
    {
      contExtra = total.vSize;
    }

  if (gOffset != kNullLocation)
    {
      gOffset += total.genSize;
    }
  else
    {
      contExtra = total.genSize;
    }

  if (qOffset != kNullLocation)
    {
      qOffset += total.qSize;
    }
  else
    {
      contExtra = total.qSize;
    }
  contOffset += total.contSize + contExtra;

  if (intOffset != kNullLocation)
    {
      intOffset += total.intSize;
    }
  else
    {
      contOffset += total.intSize;
    }

  constraintOffset += total.constraintsSize;

}

void optimOffsets::increment (optimOffsets *offsets)
{
  count_t contExtra = 0;
  if (aOffset != kNullLocation)
    {
      aOffset += offsets->total.aSize;
    }
  else
    {
      contExtra = offsets->total.aSize;
    }
  if (vOffset != kNullLocation)
    {
      vOffset += offsets->total.vSize;
    }
  else
    {
      contExtra = offsets->total.vSize;
    }

  if (gOffset != kNullLocation)
    {
      gOffset += offsets->total.genSize;
    }
  else
    {
      contExtra = offsets->total.genSize;
    }

  if (qOffset != kNullLocation)
    {
      qOffset += offsets->total.qSize;
    }
  else
    {
      contExtra = offsets->total.qSize;
    }
  contOffset += offsets->total.contSize + contExtra;

  if (intOffset != kNullLocation)
    {
      intOffset += offsets->total.intSize;
    }
  else
    {
      contOffset += offsets->total.intSize;
    }

  constraintOffset += offsets->total.constraintsSize;

}

void  optimOffsets::addSizes (optimOffsets *offsets)
{
  total.add (offsets->total);
}

void  optimOffsets::localLoad (bool finishedLoading)
{
  total = local;
  loaded = finishedLoading;
}

void optimOffsets::setOffsets (const optimOffsets &newOffsets)
{
  aOffset = newOffsets.aOffset;
  vOffset = newOffsets.vOffset;
  gOffset = newOffsets.gOffset;
  qOffset = newOffsets.qOffset;
  contOffset = newOffsets.contOffset;
  intOffset = newOffsets.intOffset;

  constraintOffset = newOffsets.constraintOffset;

  if (aOffset == kNullLocation)
    {
      aOffset = contOffset;
      contOffset += total.aSize;
    }
  if (vOffset == kNullLocation)
    {
      vOffset = contOffset;
      contOffset += total.vSize;
    }
  if (gOffset == kNullLocation)
    {
      gOffset = contOffset;
      contOffset += total.genSize;
    }
  if (qOffset == kNullLocation)
    {
      qOffset = contOffset;
      contOffset += total.qSize;
    }
  if (intOffset == kNullLocation)
    {
      intOffset = contOffset + total.contSize;
    }
}

void optimOffsets::setOffset (index_t newOffset)
{
  aOffset = newOffset;
  vOffset = aOffset + total.aSize;
  gOffset = vOffset + total.vSize;
  qOffset = gOffset + total.genSize;
  contOffset = qOffset + total.qSize;
  intOffset = contOffset + total.contSize;
}

optimOffsets * optOffsetTable::getOffsets (const optimMode &oMode)
{
  if (oMode.offsetIndex >= offsetContainer.size ())
    {
      offsetContainer.resize (oMode.offsetIndex + 1);
    }
  return &(offsetContainer[oMode.offsetIndex]);
}

static const optimOffsets nullOffsets;

const optimOffsets * optOffsetTable::getOffsets (const optimMode &oMode) const
{
  if (oMode.offsetIndex >= offsetContainer.size ())
    {
      return &nullOffsets;
    }
  return &(offsetContainer[oMode.offsetIndex]);
}

void optOffsetTable::setOffsets (const optimOffsets &newOffsets, const optimMode &oMode)
{
  if (oMode.offsetIndex >= offsetContainer.size ())
    {
      offsetContainer.resize (oMode.offsetIndex + 1);
    }
  offsetContainer[oMode.offsetIndex].setOffsets (newOffsets);
}


void optOffsetTable::setOffset (index_t newOffset, const optimMode &oMode)
{
  if (oMode.offsetIndex >= offsetContainer.size ())
    {
      offsetContainer.resize (oMode.offsetIndex + 1);
    }
  offsetContainer[oMode.offsetIndex].setOffset (newOffset);
}


void optOffsetTable::setContOffset (index_t newOffset, const optimMode &oMode)
{
  if (oMode.offsetIndex >= offsetContainer.size ())
    {
      offsetContainer.resize (oMode.offsetIndex + 1);
    }
  offsetContainer[oMode.offsetIndex].contOffset = newOffset;
}

void optOffsetTable::setIntOffset (index_t newOffset, const optimMode &oMode)
{
  if (oMode.offsetIndex >= offsetContainer.size ())
    {
      offsetContainer.resize (oMode.offsetIndex + 1);
    }
  offsetContainer[oMode.offsetIndex].intOffset = newOffset;
}

void optOffsetTable::setConstraintOffset (index_t newOffset, const optimMode &oMode)
{
  if (oMode.offsetIndex >= offsetContainer.size ())
    {
      offsetContainer.resize (oMode.offsetIndex + 1);
    }
  offsetContainer[oMode.offsetIndex].constraintOffset = newOffset;
}

index_t optOffsetTable::getaOffset (const optimMode &oMode)
{
  return offsetContainer[oMode.offsetIndex].aOffset;
}

index_t optOffsetTable::getvOffset (const optimMode &oMode)
{
  return offsetContainer[oMode.offsetIndex].vOffset;
}

index_t optOffsetTable::getContOffset (const optimMode &oMode)
{
  return offsetContainer[oMode.offsetIndex].contOffset;
}

index_t optOffsetTable::getIntOffset (const optimMode &oMode)
{
  return offsetContainer[oMode.offsetIndex].intOffset;
}

index_t optOffsetTable::getgOffset (const optimMode &oMode)
{
  return offsetContainer[oMode.offsetIndex].gOffset;
}

index_t optOffsetTable::getqOffset (const optimMode &oMode)
{
  return offsetContainer[oMode.offsetIndex].qOffset;
}

/** get the locations for the data
*@param[in] oMode the optimMode we are interested in
*/
//void getLocations (const stateData *sD, double d[], const optimMode &oMode, Lp *Loc, gridObject *obj);
/** get the locations for the data from a stateData pointer
*@param[in] oMode the optimMode we are interested in
*@return the angle offset
*/
//void getLocations (stateData *sD, double d[], const optimMode &oMode, Lp *Loc, gridObject *obj);
/** get the locations offsets for the data
*@param[in] oMode the optimMode we are interested in
*@return the angle offset
*/
//void getLocations (const optimMode &oMode, Lp *Loc);
