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

#include "sourceModels/otherSources.h"
#include "gridCoreTemplates.h"
#include "comms/gridCommunicator.h"
#include "comms/controlMessage.h"
#include "eventQueue.h"
#include "gridDyn.h"

commSource::commSource(const std::string &objName):rampSource(objName)
{
	opFlags.set(has_updates);
}

gridCoreObject * commSource::clone(gridCoreObject *obj) const
{
	commSource *cs = cloneBase<commSource, rampSource>(this, obj);
	if (cs == nullptr)
	{
		return obj;
	}
	cs->maxRamp = maxRamp;
	return cs;
}

void commSource::objectInitializeA(gridDyn_time time0, unsigned long flags)
{
	rootSim = dynamic_cast<gridSimulation *> (parent->find("root"));
	commLink = cManager.build();

	if (commLink)
	{
		commLink->initialize();
		commLink->registerReceiveCallback([this](std::uint64_t sourceID, std::shared_ptr<commMessage> message) {
			receiveMessage(sourceID, message);
		});
	}
	rampSource::objectInitializeA(time0, flags);
}

void commSource::setLevel(double val)
{
	if (opFlags[useRamp])
	{
		if (maxRamp > 0)
		{
			double dt = (val - m_output) / maxRamp;
			if (dt > 0.0001)
			{
				nextUpdateTime = prevTime + dt;
				alert(this, UPDATE_TIME_CHANGE);
				mp_dOdt = (val > m_output) ? maxRamp : -maxRamp;
			}
			else
			{
				m_output = val;
				mp_dOdt = 0.0;
			}
			
		}
		
	}
	else
	{
		m_output = val;
	}
}

void commSource::set(const std::string &param, const std::string &val)
{
	if (!(cManager.set(param, val)))
	{
		gridSource::set(param, val);
	}
}

void commSource::set(const std::string &param, double val, gridUnits::units_t unitType)
{
	if ((param == "ramp") || (param == "maxramp"))
	{
		maxRamp = std::abs(val);
	}
	else
	{
		if (!(cManager.set(param, val)))
		{
			gridSource::set(param, val, unitType);
		}
	}
}

void commSource::setFlag(const std::string &flag, bool val)
{
	if (flag == "ramp")
	{
		opFlags.set(useRamp, val);
	}
	else if (flag == "no_reply_message")
	{
		opFlags.set(no_message_reply, val);
	}
	else if (flag == "reply_message")
	{
		opFlags.set(no_message_reply, !val);
	}
	else
	{
		if (!(cManager.setFlag(flag, val)))
		{
			gridSource::setFlag(flag, val);
		}
	}
	
}

void commSource::updateA(double time)
{
	if (time > nextUpdateTime)
	{
		mp_dOdt = 0;
		nextUpdateTime = kBigNum;
	}
}

void commSource::receiveMessage(std::uint64_t sourceID, std::shared_ptr<commMessage> message)
{
	std::shared_ptr<controlMessage> m = std::dynamic_pointer_cast<controlMessage> (message);

	std::shared_ptr<controlMessage> reply;

	switch (m->getMessageType())
	{
	case controlMessage::SET:
		setLevel(m->m_value);

		if (!opFlags[no_message_reply])           //unless told not to respond return with the
		{
			auto gres = std::make_shared<controlMessage>(controlMessage::SET_SUCCESS);
			gres->m_actionID = m->m_actionID;
			commLink->transmit(sourceID, gres);
		}

		break;
	case controlMessage::GET:
		reply = std::make_shared<controlMessage>(controlMessage::GET_RESULT);
		reply->m_field = "level";
		reply->m_value = m_output;
		reply->m_time = prevTime;
		commLink->transmit(sourceID, reply);

		break;
	case controlMessage::SET_SUCCESS:
	case controlMessage::SET_FAIL:
	case controlMessage::GET_RESULT:
		break;
	case controlMessage::SET_SCHEDULED:
		if (m->m_time > prevTime)
		{
			double val = m->m_value;
			auto fea = std::make_shared<functionEventAdapter>([this, val]() {
				setLevel(val); return change_code::parameter_change;
			}, m->m_time);
			rootSim->add(fea);
		}
		else
		{
			setLevel(m->m_value);

			if (!opFlags[no_message_reply])           //unless told not to respond return with the
			{
				auto gres = std::make_shared<controlMessage>(controlMessage::SET_SUCCESS);
				gres->m_actionID = m->m_actionID;
				commLink->transmit(sourceID, gres);
			}
		}
		break;
	case controlMessage::GET_SCHEDULED:
	case controlMessage::CANCEL_FAIL:
	case controlMessage::CANCEL_SUCCESS:
	case controlMessage::GET_RESULT_MULTIPLE:
		break;
	case controlMessage::CANCEL:

		break;
	case controlMessage::GET_MULTIPLE:
		break;
	case controlMessage::GET_PERIODIC:
		break;
	default:
		break;
	}
}