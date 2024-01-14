#pragma once

namespace perf
{
	void new_frame();

	void log_section(const std::string& section_name);
	void log_slice(const std::string& slice_name);

	void draw_section_implot_graph(const std::string& section_name);
}