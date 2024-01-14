#include "stdafx.h"
#include "Performance.h"
#include <ImPlot.h>

/*

	How to use
	1. call new_frame();
	2. call log_section() for a group of similar tasks
	3. call log_slice() per each task in the section

	4. call draw() with the section name you want a stacked bar graph of.
*/

const u32 slice_count_per_section { 16 };
const u32 frame_track_count { 512 };

struct Section
{
	f32* values{ nullptr };
	std::string name { "" };
	u32 slice_count { 0 };

	std::string slice_names[slice_count_per_section];
	std::unordered_map<std::string, u32> slice_to_index{};
	Timer timer;

	void log_slice(const std::string& slice_name);

	inline Section()
	{
		values = new f32[frame_track_count * slice_count_per_section];
		memset(values, 0, sizeof(f32) * frame_track_count * slice_count_per_section);
	}
};

struct
{
	std::string current_section_name;

	std::unordered_map<std::string, usize> section_name_to_index{};
	std::vector<Section> sections{};
	u32 current_frame{ 0 };
} internal;

void Section::log_slice(const std::string& slice_name)
{
	auto slice_entry = slice_to_index.find(slice_name);
	bool entry_exists = slice_entry != slice_to_index.end();

	if (entry_exists)
	{
		values[internal.current_frame + slice_entry->second * frame_track_count] = timer.lap_delta();
	}
	else
	{
		if (slice_count == 16)
		{
			LOGERROR(std::format("\"{} \"tried to log performance slice \"{}\", went over the slice limit of {}!", name, slice_name, slice_count_per_section));
			return;
		}

		slice_names[slice_count] = (slice_name);
		slice_to_index.insert({slice_name, slice_count});
		values[slice_count_per_section * internal.current_frame + slice_count] = timer.lap_delta();
		slice_count++;
	}
}

void perf::new_frame()
{
	internal.current_frame = (internal.current_frame + 1) % frame_track_count;

	for (auto& section : internal.sections)
	{
		// Erase values from current section, as some slices might not be logged this time around
		for (u32 i = 0; i < slice_count_per_section; i++)
		{
			section.values[internal.current_frame + i * frame_track_count] = 0.0f;
		}
	}
}

void perf::log_section(const std::string& section_name)
{
	internal.sections.push_back(Section());
	internal.section_name_to_index.insert({section_name, internal.sections.size() - 1});
	auto& section = internal.sections.back();

	section.timer.start();
	section.name = section_name;
}

void perf::log_slice(const std::string& slice_name)
{
	internal.sections[internal.section_name_to_index[slice_name]].log_slice(slice_name);
}

void perf::draw_section_implot_graph(const std::string& section_name)
{
	bool section_exists = internal.section_name_to_index.find(section_name) != internal.section_name_to_index.end();

	if (!section_exists)
	{
		LOGERROR(std::format("Tried to draw non-existant performance slice \"{}\"!", section_name));
		return;
	}

	if (ImPlot::BeginPlot(section_name.c_str()))
	{
		auto& section = internal.sections[internal.section_name_to_index[section_name]];
		
		// Converting std::string to const char* for ImPlot
		std::vector<const char*> label_ids;
		for (auto& name : section.slice_names) 
			label_ids.push_back(name.data());

		ImPlot::PlotBarGroups(label_ids.data(), section.values, slice_count_per_section, frame_track_count, 1.0f, 0.0f, ImPlotBarGroupsFlags_Stacked);

		ImPlot::EndPlot();
	}
}