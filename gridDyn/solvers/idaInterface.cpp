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

#include "sundialsInterface.h"

#include "gridDyn.h"
#include "vectorOps.hpp"

#include <ida/ida.h>
#include <ida/ida_dense.h>
#include <sundials/sundials_math.h>
#include "core/helperTemplates.h"
#include "simulation/gridDynSimulationFileOps.h"
#include "sundialsMatrixData.h"
#include "matrixDataFilter.h"
#include "matrixCreation.h"

#ifdef KLU_ENABLE
#include <ida/ida_klu.h>
#include <ida/ida_sparse.h>
#endif

#include <cstdio>
#include <algorithm>
#include <string>
#include <map>
#include <cassert>


int idaFunc (realtype ttime, N_Vector state, N_Vector dstate_dt, N_Vector resid, void *user_data);
int idaJacDense (long int Neq, realtype ttime, realtype cj, N_Vector state, N_Vector dstate_dt, N_Vector resid, DlsMat J, void *user_data, N_Vector tmp1, N_Vector tmp2, N_Vector tmp3);
#ifdef KLU_ENABLE
int idaJacSparse (realtype ttime, realtype sD, N_Vector state, N_Vector dstate_dt, N_Vector resid, SlsMat J, void *user_data, N_Vector tmp1, N_Vector tmp2, N_Vector tmp3);
#endif
int idaRootFunc (realtype ttime, N_Vector state, N_Vector dstate_dt, realtype *gout, void *user_data);


idaInterface::idaInterface(const std::string &objName) : sundialsInterface(objName)
{

}

idaInterface::idaInterface (gridDynSimulation *gds, const solverMode& sMode) : sundialsInterface (gds, sMode)
{
}
idaInterface::~idaInterface ()
{
  // clear variables for IDA to use
  if (flags[initialized_flag])
    {
      IDAFree (&solverMem);
    }
}

std::shared_ptr<solverInterface> idaInterface::clone(std::shared_ptr<solverInterface> si, bool fullCopy) const
{
	auto rp = cloneBaseStack<idaInterface, sundialsInterface, solverInterface>(this, si, fullCopy);
	if (!rp)
	{
		return si;
	}

	return rp;
}

void idaInterface::allocate (count_t stateCount, count_t numRoots)
{
  // load the vectors
  if (stateCount == svsize)
    {
      return;
    }
  flags.reset(initialized_flag);
  a1.setRowLimit (stateCount);
  a1.setColLimit (stateCount);

  //update the rootCount
  rootCount = numRoots;
  rootsfound.resize (numRoots);

  //allocate the solverMemory
  if (solverMem)
    {
      IDAFree (&(solverMem));
    }
  solverMem = IDACreate ();
  check_flag(solverMem, "IDACreate", 0);

  sundialsInterface::allocate(stateCount, numRoots);
}

void idaInterface::setMaxNonZeros (count_t nonZeroCount)
{
	maxNNZ = nonZeroCount;
	jacCallCount = 0;
  a1.reserve (nonZeroCount);
  a1.clear ();
}




double idaInterface::get (const std::string &param) const
{
  long int val = -1;
  if ((param == "resevals")||(param == "iterationcount"))
    {
      IDAGetNumResEvals (solverMem, &val);
    }
  else if (param == "iccount")
    {
      val = icCount;
    }
  else if (param == "nliterations")
  {
	  IDAGetNumNonlinSolvIters(solverMem, &val);
  }
  else if (param == "jac calls")
    {
#ifdef KLU_ENABLE
      IDASlsGetNumJacEvals (solverMem, &val);
#else
      IDADlsGetNumJacEvals (solverMem, &val);
#endif
    }
  else
    {
      return sundialsInterface::get(param);
    }

  return static_cast<double> (val);
}


// output solver stats
void idaInterface::logSolverStats (print_level logLevel, bool iconly) const
{
  if (!flags[initialized_flag])
    {
      return;
    }
  long int nni = 0, nje = 0;
  int  klast, kcur;
  long int nst, nre, nreLS, netf, ncfn, nge;
  realtype tolsfac, hlast, hcur;

  std::string logstr = "";

  int retval = IDAGetNumResEvals (solverMem, &nre);
  check_flag (&retval, "IDAGetNumResEvals", 1);
  retval = IDADlsGetNumJacEvals (solverMem, &nje);
  check_flag (&retval, "IDADlsGetNumJacEvals", 1);
  retval = IDAGetNumNonlinSolvIters (solverMem, &nni);
  check_flag (&retval, "IDAGetNumNonlinSolvIters", 1);
  retval = IDAGetNumNonlinSolvConvFails (solverMem, &ncfn);
  check_flag (&retval, "IDAGetNumNonlinSolvConvFails", 1);
  if (!iconly)
    {
      retval = IDAGetNumSteps (solverMem, &nst);
      check_flag (&retval, "IDAGetNumSteps", 1);
      retval = IDAGetNumErrTestFails (solverMem, &netf);
      check_flag (&retval, "IDAGetNumErrTestFails", 1);
      retval = IDADlsGetNumResEvals (solverMem, &nreLS);
      check_flag (&retval, "IDADlsGetNumResEvals", 1);
      retval = IDAGetNumGEvals (solverMem, &nge);
      check_flag (&retval, "IDAGetNumGEvals", 1);
      retval = IDAGetCurrentOrder (solverMem, &kcur);
	  check_flag(&retval, "IDAGetCurrentOrder", 1);
      retval = IDAGetCurrentStep (solverMem, &hcur);
	  check_flag(&retval, "IDAGetCurrentStep", 1);
      retval = IDAGetLastOrder (solverMem, &klast);
	  check_flag(&retval, "IDAGetLastOrder", 1);
      retval = IDAGetLastStep (solverMem, &hlast);
	  check_flag(&retval, "IDAGetLastStep", 1);
      retval = IDAGetTolScaleFactor (solverMem, &tolsfac);
	  check_flag(&retval, "IDAGetTolScaleFactor", 1);
      logstr = "IDA Run Statistics: \n";

      logstr += "Number of steps                    = " + std::to_string (nst) + '\n';
      logstr += "Number of residual evaluations     = " + std::to_string (nre) + '\n';
      logstr += "Number of Jacobian evaluations     = " + std::to_string (nje) + '\n';
      logstr += "Number of nonlinear iterations     = " + std::to_string (nni) + '\n';
      logstr += "Number of error test failures      = " + std::to_string (netf) + '\n';
      logstr += "Number of nonlinear conv. failures = " + std::to_string (ncfn) + '\n';
      logstr += "Number of root fn. evaluations     = " + std::to_string (nge) + '\n';
      logstr += "Current order used                 = " + std::to_string (kcur) + '\n';
      logstr += "Current step                       = " + std::to_string (hcur) + '\n';
      logstr += "Last order used                    = " + std::to_string (klast) + '\n';
      logstr += "Last step                          = " + std::to_string (hlast) + '\n';
      logstr += "Tolerance scale factor             = " + std::to_string (tolsfac) + '\n';
    }
  else
    {
      logstr = "IDACalcIC Statistics: \n";
      logstr += "Number of residual evaluations     = " + std::to_string (nre) + '\n';
      logstr += "Number of Jacobian evaluations     = " + std::to_string (nje) + '\n';
      logstr += "Number of nonlinear iterations     = " + std::to_string (nni) + '\n';
      logstr += "Number of nonlinear conv. failures = " + std::to_string (ncfn) + '\n';
    }

  if (m_gds)
    {
      m_gds->log (m_gds, logLevel, logstr);
    }
  else
    {
      printf ("\n%s", logstr.c_str ());
    }
}


void idaInterface::logErrorWeights (print_level logLevel) const
{

  N_Vector eweight = NVECTOR_NEW (use_omp, svsize);
  N_Vector ele = NVECTOR_NEW(use_omp, svsize);

  realtype *eldata = NVECTOR_DATA (use_omp, ele);
  realtype *ewdata = NVECTOR_DATA (use_omp, eweight);
  IDAGetErrWeights (solverMem, eweight);
  IDAGetEstLocalErrors (solverMem, ele);
  std::string logstr = "Error Weight\tEstimated Local Errors\n";
  for (count_t kk = 0; kk < svsize; ++kk)
    {
      logstr += std::to_string (kk) + ':' + std::to_string (ewdata[kk]) + '\t' + std::to_string ( eldata[kk]) + '\n';
    }

  if (m_gds)
    {
      m_gds->log (m_gds, logLevel, logstr);
    }
  else
    {
      printf ("\n%s", logstr.c_str ());
    }
  NVECTOR_DESTROY(use_omp, eweight);
  NVECTOR_DESTROY(use_omp, ele);
}

/* *INDENT-OFF* */
static const std::map<int, std::string> idaRetCodes {
  {IDA_MEM_NULL, "The solver memory argument was NULL"},
  {IDA_ILL_INPUT, "One of the function inputs is illegal"},
  {IDA_NO_MALLOC, "The solver memory was not allocated by a call to IDAMalloc"},
  {IDA_TOO_MUCH_WORK, "The solver took mxstep internal steps but could not reach tout"},
  {IDA_TOO_MUCH_ACC, "The solver could not satisfy the accuracy demanded by the user for some internal step"},
  {IDA_ERR_FAIL, "Error test failures occurred too many times during one internal time step or minimum step size was reached"},
  {IDA_CONV_FAIL, "Convergence test failures occurred too many times during one internal time step or minimum step size was reached"},
  {IDA_LINIT_FAIL, "The linear solver's initialization function failed"},
  {IDA_LSETUP_FAIL, "The linear solver's setup function failed in an unrecoverable manner"},
  {IDA_LSOLVE_FAIL, "The linear solver's solve function failed in an unrecoverable manner"},
  {IDA_RES_FAIL, "The user - provided residual function failed in an unrecoverable manner"},
  {IDA_CONSTR_FAIL, "12	The inequality constraints were violated and the solver was unable to recover"},
  {IDA_MEM_FAIL, "14	A memory allocation failed"},
  {IDA_BAD_T, "The time t is outside the last step taken"},
  {IDA_BAD_EWT, "Zero value of some error weight component"},
  {IDA_FIRST_RES_FAIL, "The user - provided residual function failed recoverably on the first call"},
  {IDA_LINESEARCH_FAIL, "The line search failed"},
  {IDA_NO_RECOVERY, "The residual function, linear solver setup function, or linear solver solve function had a recoverable failure, but IDACalcIC could not recover"},
  {IDA_RTFUNC_FAIL, "The rootfinding function failed in an unrecoverable manner"},
  {IDA_REP_RES_ERR, "The user-provided residual function repeatedly returned a recoverable error flag, but the solver was unable to recover"},
  {IDA_MEM_FAIL, "Memory Allocation failed"},
  {IDA_BAD_K, "Bad K"},
  {IDA_BAD_DKY, "Bad DKY"},
};
/* *INDENT-ON* */

void idaInterface::initialize (coreTime t0)
{
  if (!flags[allocated_flag])
    {
	  throw(InvalidSolverOperation());
    }
  auto jsize = m_gds->jacSize (mode);

  // dynInitializeB IDA - Sundials

  int retval = IDASetUserData (solverMem, static_cast<void *>(this));
  check_flag(&retval, "IDASetUserData", 1);

  //guess an initial condition
  m_gds->guess (t0, state_data(), deriv_data(),mode);

  retval = IDAInit (solverMem, idaFunc, t0, state, dstate_dt);
  check_flag(&retval, "IDAInit", 1);

  if (rootCount > 0)
    {
      rootsfound.resize (rootCount);
      retval = IDARootInit (solverMem, rootCount, idaRootFunc);
	  check_flag(&retval, "IDARootInit", 1);
       
    }

  N_VConst (tolerance, abstols);

  retval = IDASVtolerances (solverMem, tolerance / 100, abstols);
  check_flag(&retval, "IDASVtolerances", 1);

  retval = IDASetMaxNumSteps (solverMem, 1500);
  check_flag(&retval, "IDASetMaxNumSteps", 1);
#ifdef KLU_ENABLE
  if (flags[dense_flag])
    {
      retval = IDADense (solverMem, svsize);
	  check_flag(&retval, "IDADense", 1);

      retval = IDADlsSetDenseJacFn (solverMem, idaJacDense);
	  check_flag(&retval, "IDADlsSetDenseJacFn", 1);
     
    }
  else
    {

      retval = IDAKLU (solverMem, svsize, jsize,CSR_MAT);
	  check_flag(&retval, "IDAKLU", 1);

      retval = IDASlsSetSparseJacFn (solverMem, idaJacSparse);
	  check_flag(&retval, "IDASlsSetSparseJacFn", 1);
    }
#else
  retval = IDADense (solverMem, svsize);
  check_flag(&retval, "IDADense", 1);

  retval = IDADlsSetDenseJacFn (solverMem, idaJacDense);
  check_flag(&retval, "IDADlsSetDenseJacFn", 1);
  #endif





  retval = IDASetMaxNonlinIters (solverMem, 20);
  check_flag(&retval, "IDASetMaxNonlinIters", 1);

  m_gds->getVariableType (type_data (), mode);

  retval = IDASetId (solverMem, types);
  check_flag(&retval, "IDASetId", 1);

  retval = IDASetErrHandlerFn (solverMem, sundialsErrorHandlerFunc, (void *)this);
  check_flag(&retval, "IDASetErrHandlerFn", 1);

  setConstraints ();
  solveTime = t0;
  flags.set(initialized_flag);

}

void idaInterface::sparseReInit (sparse_reinit_modes sparseReinitMode)
{
#ifdef KLU_ENABLE
  if (flags[dense_flag])
    {
	  return;
    }
  else
    {
      int kinmode = (sparseReinitMode == sparse_reinit_modes::refactor) ? 1 : 2;
      int retval = IDAKLUReInit (solverMem, static_cast<int> (svsize), static_cast<int> (a1.capacity ()), kinmode);
	  check_flag(&retval, "IDAKLUReInit", 1);
	  jacCallCount = 0;
	  
    }
#endif
}



void idaInterface::setRootFinding (count_t numRoots)
{
  if (numRoots != rootsfound.size ())
    {
      rootsfound.resize (numRoots);
    }
  rootCount = numRoots;
  int retval = IDARootInit (solverMem, numRoots, idaRootFunc);
  check_flag(&retval, "IDARootInit", 1);
  
}

#define SHOW_MISSING_ELEMENTS 0

int idaInterface::calcIC (coreTime t0, coreTime tstep0, ic_modes initCondMode, bool constraints)
{
  int retval;
  ++icCount;
  assert (icCount < 200);
  if (initCondMode == ic_modes::fixed_masked_and_deriv) //mainly for use upon startup from steady state
    {
      //do a series of steps to ensure the original algebraic states are fixed and the derivatives are fixed
	  flags.set(useMask_flag);
      loadMaskElements ();
      if (!flags[dense_flag])
        {
          sparseReInit (sparse_reinit_modes::refactor);
        }
      retval = IDACalcIC (solverMem, IDA_Y_INIT, t0 + tstep0); //IDA_Y_INIT

      //retval = IDACalcIC (solverMem, IDA_YA_YDP_INIT, t0 + tstep0); //IDA_YA_YDP_INIT
      //   getCurrentData();
      //  printStates(true);
      if (retval!=0)
        {
          //if the solver failed with error code -14 then we probably have a singular matrix
          //then locate the singular elements and fix them so the problem is valid
          if (retval == IDA_NO_RECOVERY)
            {
              auto mvec = findMissing (a1);
              if (mvec.size () > 0)
                {
                  double *lstate = NV_DATA_S (state);
                  for (auto &me : mvec)
                    {
                      maskElements.push_back (me);
                      tempState[me] = lstate[me];
                    }

                  if (!flags[dense_flag])
                    {
                      sparseReInit (sparse_reinit_modes::refactor);
                    }
                  retval = IDACalcIC (solverMem, IDA_Y_INIT, t0 + tstep0); //IDA_Y_INIT
                  if (retval == IDA_SUCCESS)
                    {
                      return FUNCTION_EXECUTION_SUCCESS;
                    }
                  else
                    {
                      return SOLVER_INVALID_STATE_ERROR;
                    }
                }
            }
          else
            {
              switch (retval)
                {
                case IDA_SUCCESS:                 //no error
                  break;
                case IDA_REP_RES_ERR:
                case IDA_NO_RECOVERY:
                  retval = SOLVER_INVALID_STATE_ERROR;
                  break;
                default:
                  break;
                }
              return retval;
            }
        }
	  flags.reset(useMask_flag);
      if (!flags[dense_flag])
        {
          sparseReInit (sparse_reinit_modes::refactor);
        }
    }
  else if (initCondMode == ic_modes::fixed_diff)
    {
      retval = IDAReInit (solverMem, t0, state, dstate_dt);

      if (retval<0)
        {

          return retval;
        }
      if (constraints)
        {
          setConstraints ();
        }
      //  printStates();
      retval = IDACalcIC (solverMem, IDA_YA_YDP_INIT, t0 + tstep0); //IDA_YA_YDP_INIT
      if (retval<0)
        {
#if SHOW_MISSING_ELEMENTS > 0
          auto mvec = findMissing (&a1);
          if (mvec.size () > 0)
            {
              printf ("missing rows in Jacobian from calcIC mode 1\n");
            }
#endif
          switch (retval)
            {
            case IDA_REP_RES_ERR:
            case IDA_NO_RECOVERY:
              retval = SOLVER_INVALID_STATE_ERROR;
              break;
            default:
              break;
            }
          return retval;
        }
      // getCurrentData();
      //  printStates();
    }
  return FUNCTION_EXECUTION_SUCCESS;
}

void idaInterface::getCurrentData ()
{
  int retval = IDAGetConsistentIC (solverMem, state, dstate_dt);
  check_flag(&retval, "IDAGetConsistentIC", 1);
}

int idaInterface::solve (coreTime tStop, coreTime &tReturn, step_mode stepMode)
{
  assert (rootCount == m_gds->rootSize (mode));
  ++solverCallCount;
  icCount = 0;
  double tret;
  int retval = IDASolve (solverMem, tStop, &tret, state, dstate_dt, (stepMode == step_mode::normal) ? IDA_NORMAL : IDA_ONE_STEP);
  tReturn = tret;
  switch (retval)
    {
    case IDA_SUCCESS:       //no error
      break;
    case IDA_ROOT_RETURN:
      retval = SOLVER_ROOT_FOUND;
      break;
    case IDA_REP_RES_ERR:
      retval = SOLVER_INVALID_STATE_ERROR;
      break;
    default:
      break;
    }
  return retval;
}

void idaInterface::getRoots ()
{
  int ret = IDAGetRootInfo (solverMem, rootsfound.data ());
  check_flag(&ret, "IDAGetRootInfo", 1);

}

void idaInterface::setConstraints ()
{
  if (m_gds->hasConstraints ())
    {
      N_VConst (ZERO, consData);
      m_gds->getConstraints (NVECTOR_DATA (use_omp,consData), mode);
      IDASetConstraints (solverMem, consData);
    }
}

void idaInterface::loadMaskElements ()
{
  std::vector<double> mStates (svsize,0.0);
  m_gds->getVoltageStates (mStates.data (),mode);
  m_gds->getAngleStates (mStates.data (),mode);
  maskElements = vecFindgt<double, index_t> (mStates,0.5);
  tempState.resize (svsize);
  double *lstate = NV_DATA_S (state);
  for (auto &v : maskElements)
    {
      tempState[v] = lstate[v];
    }
}


// IDA C Functions
int idaFunc (realtype ttime, N_Vector state, N_Vector dstate_dt, N_Vector resid, void *user_data)
{
  idaInterface *sd = reinterpret_cast<idaInterface *> (user_data);
  //printf("time=%f\n", ttime);
  int ret = sd->m_gds->residualFunction (ttime, NVECTOR_DATA (sd->use_omp, state), NVECTOR_DATA (sd->use_omp, dstate_dt), NVECTOR_DATA (sd->use_omp, resid), sd->mode);
  if (sd->flags[useMask_flag])
    {
      double *lstate = NVECTOR_DATA (sd->use_omp, state);
      double *lresid = NVECTOR_DATA (sd->use_omp, resid);
      for (auto &v:sd->maskElements)
        {
          lresid[v] = 100.0 * (lstate[v] - sd->tempState[v]);
        }
    }
	if (sd->flags[fileCapture_flag])
	{
		if (!sd->stateFile.empty())
		{
			writeVector(sd->solveTime, STATE_INFORMATION, sd->funcCallCount, sd->mode.offsetIndex, sd->svsize, NVECTOR_DATA(sd->use_omp, state), sd->stateFile, (sd->funcCallCount != 1));
			writeVector(sd->solveTime, DERIVATIVE_INFORMATION, sd->funcCallCount, sd->mode.offsetIndex, sd->svsize, NVECTOR_DATA(sd->use_omp, dstate_dt), sd->stateFile);
			writeVector(sd->solveTime, RESIDUAL_INFORMATION, sd->funcCallCount, sd->mode.offsetIndex, sd->svsize, NVECTOR_DATA(sd->use_omp, resid), sd->stateFile);
		}
}

  return ret;
}

int idaRootFunc (realtype ttime, N_Vector state, N_Vector dstate_dt, realtype *gout, void *user_data)
{
  idaInterface *sd = reinterpret_cast<idaInterface *> (user_data);
  sd->m_gds->rootFindingFunction (ttime, NVECTOR_DATA(sd->use_omp, state), NVECTOR_DATA(sd->use_omp, dstate_dt), gout, sd->mode);

  return FUNCTION_EXECUTION_SUCCESS;
}

#define CHECK_JACOBIAN 0
int idaJacDense (long int Neq, realtype ttime, realtype cj, N_Vector state, N_Vector dstate_dt, N_Vector /*resid*/, DlsMat J, void *user_data, N_Vector tmp1, N_Vector tmp2, N_Vector tmp3)
{
	return sundialsJacDense(Neq, ttime, cj, state, dstate_dt, J, user_data, tmp1, tmp2, tmp3);
}

//#define CAPTURE_JAC_FILE

#ifdef KLU_ENABLE
int idaJacSparse (realtype ttime, realtype cj, N_Vector state, N_Vector dstate_dt, N_Vector /*resid*/, SlsMat J, void *user_data, N_Vector tmp1, N_Vector tmp2, N_Vector tmp3)
{
	return sundialsJacSparse(ttime, cj, state, dstate_dt, J, user_data, tmp1, tmp2, tmp3);
  
}
#endif
