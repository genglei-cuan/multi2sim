/*
 * Multi2Sim
 * Copyright (C) 2014 Agamemnon Despopoulos (agdespopoulos@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <algorithm>
#include <vector>

#include <lib/esim/Engine.h>

#include "Address.h"
#include "Bank.h"
#include "Channel.h"
#include "Controller.h"
#include "Command.h"
#include "Request.h"
#include "Scheduler.h"
#include "System.h"
#include "TimingParameters.h"


namespace dram
{

// Static variables

misc::StringMap PagePolicyTypeMap
{
	{ "Open", PagePolicyOpen},
	{ "Closed", PagePolicyClosed }
};

std::map<int, esim::EventType *> Controller::REQUEST_PROCESSORS;


Controller::Controller(int id)
		:
		id(id)
{
	// Create a new request processor event for this controller.
	CreateRequestProcessor(id);
}


Controller::Controller(int id, const std::string &section,
		misc::IniFile &config)
		:
		Controller(id)
{
	ParseConfiguration(section, config);
}


void Controller::ParseConfiguration(const std::string &section,
		misc::IniFile &config)
{
	// Tokenize the section title
	std::vector<std::string> section_tokens;
	misc::StringTokenize(section, section_tokens);

	// Load the name of the Controller
	// Format of section title is "MemoryController <name>"
	name = section_tokens[1];

	// Load the page policy, defaulting to PagePolicyOpen
	page_policy = (PagePolicyType) config.ReadEnum(section, "PagePolicy",
			PagePolicyTypeMap, PagePolicyOpen);

	// Load the scheduling algorithm, defaulting to SchedulerOldestFirst.
	SchedulerType scheduler_type = (SchedulerType) config.ReadEnum(section,
			"SchedulingPolicy", SchedulerTypeMap,
			SchedulerOldestFirst);

	// Read DRAM size settings
	num_channels = config.ReadInt(section, "NumChannels", 1);
	num_ranks = config.ReadInt(section, "NumRanks", 2);
	// int num_devices = config.ReadInt(section, "NumDevices", 8);
	num_banks = config.ReadInt(section, "NumBanks", 8);
	num_rows = config.ReadInt(section, "NumRows", 1024);
	num_columns = config.ReadInt(section, "NumColumns", 1024);
	num_bits = config.ReadInt(section, "NumBits", 8);

	// Create channels 
	for (int i = 0; i < num_channels; i++)
		channels.emplace_back(new Channel(i, this, num_ranks,
				num_banks, num_rows, num_columns, num_bits,
				scheduler_type));

	// Read DRAM timing parameters
	ParseConfigurationTiming(section, config);

	// Create a set of new scheduler events for all the channels.
	CreateSchedulers(num_channels);
}


void Controller::ParseConfigurationTiming(const std::string &section,
		misc::IniFile &ini_file)
{
	// Create a timings object to use to read the parameters and pass it
	// the configuration file and section to parse.
	TimingParameters parameters(section, ini_file);

	// Build the timing matrix.
	// A A s s
	timings[TimingActivate][TimingActivate][TimingSame][TimingSame] =
			parameters.tRC;
	// A A s d
	timings[TimingActivate][TimingActivate][TimingSame][TimingDifferent] =
			parameters.tRRD;
	// P A s s
	timings[TimingPrecharge][TimingActivate][TimingSame][TimingSame] =
			parameters.tRP;
	// A R s s
	timings[TimingActivate][TimingRead][TimingSame][TimingSame] =
			parameters.tRCD;
	// R R s a
	timings[TimingRead][TimingRead][TimingSame][TimingAny] =
			std::max(parameters.tBURST, parameters.tCCD);
	// R R d a
	timings[TimingRead][TimingRead][TimingDifferent][TimingAny] =
			parameters.tBURST + parameters.tRTRS;
	// W R s a
	timings[TimingWrite][TimingRead][TimingSame][TimingAny] =
			parameters.tCWD + parameters.tBURST + parameters.tWTR;
	// W R d a
	timings[TimingWrite][TimingRead][TimingDifferent][TimingAny] =
			parameters.tCWD + parameters.tBURST + parameters.tRTRS - parameters.tCAS;
	// A W s s
	timings[TimingActivate][TimingWrite][TimingSame][TimingSame] =
			parameters.tRCD;
	// R W a a
	timings[TimingRead][TimingWrite][TimingAny][TimingAny] =
			parameters.tCAS + parameters.tBURST + parameters.tRTRS - parameters.tCWD;
	// W W s a
	timings[TimingWrite][TimingWrite][TimingSame][TimingAny] =
			std::max(parameters.tBURST, parameters.tCCD);
	// W W d a
	timings[TimingWrite][TimingWrite][TimingDifferent][TimingAny] =
			parameters.tBURST + parameters.tOST;
	// A P s s
	timings[TimingActivate][TimingPrecharge][TimingSame][TimingSame] =
			parameters.tRAS;
	// R P s s
	timings[TimingRead][TimingPrecharge][TimingSame][TimingSame] =
			parameters.tBURST + parameters.tRTP - parameters.tCCD;
	// W P s s
	timings[TimingWrite][TimingPrecharge][TimingSame][TimingSame] =
			parameters.tCWD + parameters.tBURST + parameters.tWR;

	// Store command durations.
	command_durations[CommandPrecharge] = parameters.tRP;
	command_durations[CommandActivate] = parameters.tRCD;
	command_durations[CommandRead] = parameters.tCAS + parameters.tBURST;
	command_durations[CommandWrite] =
			parameters.tCWD + parameters.tBURST + parameters.tWTR;
}


void Controller::AddRequest(std::shared_ptr<Request> request)
{
	// Add the request to the controller incoming request queue.
	incoming_requests.push(request);

	// Ensure the request processor is running.
	CallRequestProcessor();
}


void Controller::CreateRequestProcessor(int controller)
{
	esim::Engine *esim = esim::Engine::getInstance();

	// Create a new EventType for this controller's request processor
	// and store it in the map.
	REQUEST_PROCESSORS[controller] = esim->RegisterEventType(
			misc::fmt("%d_REQUEST_PROCESSOR", controller),
			Controller::RequestProcessorHandler,
			System::DRAM_DOMAIN);
}


void Controller::CreateSchedulers(int num_channels)
{
	esim::Engine *esim = esim::Engine::getInstance();

	// Create a new EventType for each channel's scheduler for this
	// controller and store them in the map.
	for (int i = 0; i < num_channels; i++)
		SCHEDULERS[i] = esim->RegisterEventType(
				misc::fmt("%d_%d_SCHEDULER", id, i),
				Channel::SchedulerHandler,
				System::DRAM_DOMAIN);
}


void Controller::CallRequestProcessor()
{
	// Get the esim engine instance.
	esim::Engine *esim = esim::Engine::getInstance();

	// Get the request processor event for this controller.
	esim::EventType *request_processor = getRequestProcessor(id);

	// Check that the event isn't already scheduled.
	if (request_processor->isInFlight())
	{
		return;
	}

	// Create the frame to pass containing a reference to this controller.
	auto frame = std::make_shared<RequestProcessorFrame>();
	frame->controller = this;

	// Call the event for the request processor.
	esim->Call(request_processor, frame, nullptr, 1);
}


void Controller::RequestProcessorHandler(esim::EventType *type,
		esim::EventFrame *frame)
{
	// Get the controller pointer out of the frame.
	RequestProcessorFrame *request_frame =
			dynamic_cast<RequestProcessorFrame *>(frame);
	Controller *controller = request_frame->controller;

	// Call this controller's request processor.
	controller->RunRequestProcessor();
}


void Controller::RunRequestProcessor()
{
	// Just in case, make sure there are actually requests to process.
	if (incoming_requests.size() == 0)
		return;

	// Get the front request in the queue.
	std::shared_ptr<Request> request = incoming_requests.front();

	// Get the bank the request is destined for.
	Address *address = request->getAddress();
	Bank *bank = channels[address->getLogical()]->
			getRank(address->getRank())->
			getBank(address->getBank());

	// Send the request to the bank to be processed.
	bank->ProcessRequest(request);

	// Remove the request from the queue.
	incoming_requests.pop();

	// If there are still requests to be processed, schedule again next cycle.
	if (incoming_requests.size() > 0)
		CallRequestProcessor();
}


void Controller::CommandReturnHandler(esim::EventType *type,
		esim::EventFrame *frame)
{
	// Get the command pointer out of the frame.
	CommandReturnFrame *command_frame =
			dynamic_cast<CommandReturnFrame *>(frame);
	Command *command = command_frame->getCommand();

	// The command is finished, so mark it as such.  This also triggers
	// the command to decrement the number of in flight commands for
	// its request.
	command->setFinished();

	// Debug
	long long cycle = System::DRAM_DOMAIN->getCycle();
	System::activity << misc::fmt("[%lld] [%d : %d] Finished command #%d "
			"%s for 0x%llx\n", cycle, command->getRankId(),
			command->getBankId(), command->getId(),
			command->getTypeString().c_str(),
			command->getAddress()->getEncoded());
}


void Controller::dump(std::ostream &os) const
{
	// Print header
	os << misc::fmt("Dumping Controller %d (%s)\n", id, name.c_str());

	// Print the requests currently in queue
	os << misc::fmt("%d requests in the incoming queue\n",
			(int) incoming_requests.size());

	// Print channels owned by this controller
	os << misc::fmt("%d Channels\nChannel dump:\n", (int) channels.size());
	for (auto const& channel : channels)
	{
		channel->dump(os);
	}
}


}  // namespace dram
