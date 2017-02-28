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

#ifndef GRIDDYN_RELAY_H_
#define GRIDDYN_RELAY_H_

#include "gridObjects.h"
#include "core/objectOperatorInterface.h"
#include "comms/commManager.h"

class stateGrabber;
class gridGrabber;
class gridCondition;
class gridCommunicator;
class eventAdapter;
class gridEvent;
class commMessage;
class propertyBuffer;

enum class change_code; //forward declare change_code enumeration

/**
*@brief relay class:
 relay's are sensors and actuators.  They can read data from coreObjects and then take actions on other
* objects on a regular schedule or on a functional basis.
**/
class gridRelay : public gridPrimary, objectOperatorInterface
{
public:
  static std::atomic<count_t> relayCount;  //!< counter for the number of relays
  /** @brief enumeration of the relay condition states*/
  enum class condition_states
  {
    active, //!< the relay condition is active
    triggered,  //!< the relay condition is triggered and waiting a timeout
    disabled,  //!< the relay condition is disabled and not scanning
  };

protected:
  
  enum relay_flags
  {
    continuous_flag = object_flag1,             //!< flag indicating the relay has some continuous checks
    resettable_flag = object_flag2,             //!< flag indicating that the conditions can be reset
    use_commLink = object_flag3,             //!< flag indicating that the relay uses communications
    power_flow_checks_flag = object_flag4,             //!< flag indicating that the relay should be in operation during power flow
    extra_relay_flag = object_flag5,             //!< just defining an extra name for additional relay flags

  };
  coreTime triggerTime = maxTime;            //!<the next time execute
  coreObject *m_sourceObject = nullptr;       //!<the default object where the data comes from
  coreObject *m_sinkObject = nullptr;            //!<the default object where the actions occur
  count_t triggerCount = 0;        //!< count of the number of triggers
  count_t actionsTakenCount = 0;        //!< count of the number of actions taken

  // comm fields
  commManager cManager;    //!< structure object to store and manage the communicator information
  
  std::shared_ptr<gridCommunicator> commLink;             //!<communicator link

  coreTime m_nextSampleTime = maxTime;        //!< the next time to sample the conditions

public:
  explicit gridRelay (const std::string &objName = "relay_$");

  virtual coreObject * clone (coreObject *obj = nullptr) const override;

  virtual void add (coreObject *obj) override;

  /**
  *@brief add a gridEvent to the relay
  **/
  virtual void add (std::shared_ptr<gridEvent> ge);
  /**
  *@brief add an EventAdapter to the relay
  **/
  virtual void add (std::shared_ptr<eventAdapter> geA);
  /**
  * @brief add a condition to the relay
  **/
  virtual void add (std::shared_ptr<gridCondition> gc);

  /**
  *@brief update a specific action
  **/
  virtual void updateAction (std::shared_ptr<gridEvent> ge, index_t actionNumber);
  /**
  *@brief update a specific action
  **/
  virtual void updateAction (std::shared_ptr<eventAdapter> geA, index_t actionNumber);
  /**
  *@brief update a specific condition
  **/
  virtual void updateCondition (std::shared_ptr<gridCondition> gc,  index_t conditionNumber);

  /**
  *@brief reset the relay
  **/
  void resetRelay ();
  /**
  * @brief set the relay source object
  */
  void setSource (coreObject *obj);
  /**
  * @brief set the relay sink object
  */
  void setSink (coreObject *obj);

  void setConditionState (index_t conditionNumber,condition_states newState = condition_states::active);
  void removeAction (index_t conditionNumber);

  std::shared_ptr<gridCondition> getCondition (index_t conditionNumber);
  std::shared_ptr<eventAdapter> getAction (index_t actionNumber);

  condition_states getConditionStatus (index_t conditionNumber);
  double getConditionValue (index_t conditionNumber) const;
  double getConditionValue (index_t conditionNumber, const stateData &sD, const solverMode &sMode) const;
  bool checkCondition (index_t conditionNumber) const;
  void setConditionLevel (index_t conditionNumber, double levelVal);
  virtual void setActionTrigger (index_t conditionNumber, index_t actionNumber, coreTime delayTime = timeZero);
  virtual change_code triggerAction (index_t actionNumber);
  virtual void setActionMultiTrigger (const IOlocs &multi_conditions, index_t actionNumber, coreTime delayTime = timeZero);

  void setResetMargin (index_t conditionNumber, double margin);
  virtual void setFlag (const std::string &flag, bool val = true)  override;
  virtual void set (const std::string &param,  const std::string &val) override;

  virtual void set (const std::string &param, double val, gridUnits::units_t unitType = gridUnits::defUnit)  override;

  virtual void updateA (coreTime time)  override;
  virtual void pFlowObjectInitializeA (coreTime time0, unsigned long flags) override;
  virtual void dynObjectInitializeA (coreTime time0, unsigned long flags)  override;
  virtual change_code powerFlowAdjust (const IOdata &inputs, unsigned long flags, check_level_t level) override;
  virtual void rootTest (const IOdata &inputs, const stateData &sD, double roots[], const solverMode &sMode)  override;
  virtual void rootTrigger (coreTime ttime, const IOdata &inputs, const std::vector<int> &rootMask, const solverMode &sMode)  override;
  virtual change_code rootCheck (const IOdata &inputs, const stateData &sD, const solverMode &sMode,  check_level_t level)  override;
  /** message processing function for use with communicators
  @param[in] sourceID  the source of the comm message
  @param[in] message the actual message to process
  */
  virtual void receiveMessage (std::uint64_t sourceID, std::shared_ptr<commMessage> message);
  /** send and alarm message
  @param[in] code the code to put in the alarm message
  @return FUNCTION_EXECUTION_SUCCESS if successful <0 if not
  */
  int sendAlarm (std::uint32_t code);
  /** generate an alarm event
  @param[in] val a string defining the alarm
  */
  std::unique_ptr<eventAdapter> make_alarm (const std::string &val);
  //Object operator interface functions

  virtual void updateObjectLinkages(coreObject *newRoot) override;
  virtual void updateObject(coreObject *obj, object_update_mode mode = object_update_mode::direct) override;
  virtual coreObject * getObject() const override;
  virtual void getObjects(std::vector<coreObject *> &objects) const override;
protected:
	/** update the number of root finding functions used in the relay
	@param[in] alertChange true if the function should send alerts to its parent object if the number of roots changes
	*/
  virtual void updateRootCount (bool alertChange = true);
  /** do something when an action is taken 
  @param ActionNum  the index of the action that was executed
  @param conditionNum the index of the condition that triggered the action
  @param actionReturn  the return code of the action execution
  @param actionTime the time at which the action was taken
  */
  virtual void actionTaken (index_t ActionNum, index_t conditionNum,  change_code actionReturn, coreTime actionTime);
  /** do something when an condition is triggered
  @param conditionNum the index of the condition that triggered the action
  @param timeTriggered the time at which the condition was triggered
  */
  virtual void conditionTriggered (index_t conditionNum, coreTime timeTriggered);
  /** do something when an condition is cleared
  @param conditionNum the index of the condition that triggered the action
  @param timeCleared the time at which the condition was cleared
  */
  virtual void conditionCleared (index_t conditionNum, coreTime timeCleared);

  /** generate the commlink name*/
  virtual std::string generateCommName();

private:
  /** @brief subclass  data container for helping with condition time checks*/
  class condCheckTime
  {
public:
    index_t conditionNum;                      //!< the condition Number
    index_t actionNum;                     //!< the action number
	coreTime testTime;                      //!< the time the test should be performed
    bool multiCondition = false;                      //!< flag if the condition is part of a multiCondition

    /** @brief constructor with all the data
    @param[in] cNum the conditionNumber
    @param[in] aNum the actionNumber
    @param[in] ttime the time to conduct a test
    @param[in] mcond  the value of the multiCondition flag
    */
    condCheckTime (index_t cNum=0, index_t aNum=0, coreTime ttime=maxTime, bool mcond = false) : conditionNum (cNum), actionNum (aNum), testTime (ttime), multiCondition (mcond)
    {
    }
  };
  /** @brief data type declaration for a multiCondition trigger*/
 class mcondTrig
  {
  public:
	 index_t actionNum=kInvalidLocation;                      //!< the related ActionNumber
	 IOlocs multiConditions;   //!< identification of all the conditions involved
    coreTime delayTime=timeZero;   //!< the delay time all conditions must be true before the action is taken TODO:PT account for this delay
	mcondTrig() {};
	mcondTrig(index_t actNum, const IOlocs &conds, coreTime delTime = timeZero) :actionNum(actNum), multiConditions(conds), delayTime(delTime)
	{};
 };
  /** enumeration of relay flags*/
 count_t numAlgRoots = 0;        //!< counter for the number of root finding operations related to the condition checking
  std::vector < std::shared_ptr < gridCondition >> conditions;                //!<state conditionals for the system
  std::vector < std::shared_ptr < eventAdapter >> actions;                //!<actions to take in response to triggers
  std::vector < std::vector < index_t >> actionTriggers;                //!<the conditions that cause actions
  std::vector < std::vector < coreTime >> actionDelays;               //!<the periods of time in which the condition must be true for an action to occur
  std::vector<condition_states> cStates;               //!< a vector of states for the conditions
  std::vector<coreTime> conditionTriggerTimes;               //!< the times at which the condition triggered
  std::vector<condCheckTime> condChecks;               //!<a vector of condition action pairs that are in wait and see mode
  std::vector < std::vector < mcondTrig >> multiConditionTriggers;               //!<a vector for action which have multiple triggers
  std::vector<index_t> conditionsWithRoots;			//!< indices of the conditions with root finding functions attached to them
private:
 
  void clearCondChecks (index_t condNum);
  change_code executeAction (index_t actionNum, index_t conditionNum, coreTime actionTime);
  /** trigger a specific condition
  @param[in] conditionNum  the index of the condition to trigger
  @param[in] conditionTriggerTime the time of the trigger
  @param[in] minimumDelayTime  ignore all trigger delays below the minimumDelayTime
  */
  change_code triggerCondition (index_t conditionNum,coreTime conditionTriggerTime, coreTime minimumDelayTime);
  
  /** check and if all conditions hold execute a multi-condition trigger
  @param[in] conditionNum  the index of the condition that was just triggered that might also trigger a multi-condition
  @param[in] conditionTriggerTime the time of the trigger
  @param[in] minimumDelayTime  ignore all trigger delays below the minimumDelayTime
  */
  change_code multiConditionCheckExecute (index_t conditionNum, coreTime conditionTriggerTime, coreTime minimumDelayTime);
  change_code evaluateCondCheck (condCheckTime &cond, coreTime currentTime);
};



#endif