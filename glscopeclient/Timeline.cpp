/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2019 Andrew D. Zonenberg                                                                          *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/
#include "glscopeclient.h"
#include "WaveformGroup.h"
#include "Timeline.h"
#include "WaveformArea.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

Timeline::Timeline()
{
	m_dragState = DRAG_NONE;
	m_dragStartX = 0;
	m_originalTimeOffset = 0;

	set_size_request(1, 40);

	add_events(
		Gdk::POINTER_MOTION_MASK |
		Gdk::BUTTON_PRESS_MASK |
		Gdk::BUTTON_RELEASE_MASK);
}

Timeline::~Timeline()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI events

bool Timeline::on_button_press_event(GdkEventButton* event)
{
	//for now, only handle left click
	if(event->button == 1)
	{
		m_dragState = DRAG_TIMELINE;
		m_dragStartX = event->x;
		m_originalTimeOffset = m_group->m_timeOffset;
	}

	return true;
}

bool Timeline::on_button_release_event(GdkEventButton* event)
{
	if(event->button == 1)
	{
		m_dragState = DRAG_NONE;
	}
	return true;
}

bool Timeline::on_motion_notify_event(GdkEventMotion* event)
{
	switch(m_dragState)
	{
		//Dragging the horizontal offset
		case DRAG_TIMELINE:
			{
				double dx = event->x - m_dragStartX;
				double ps = dx / m_group->m_pixelsPerPicosecond;

				//Update offset, but don't allow scrolling before the start of the capture
				m_group->m_timeOffset = m_originalTimeOffset - ps;
				if(m_group->m_timeOffset < 0)
					m_group->m_timeOffset = 0;

				m_group->m_frame.queue_draw();
			}
			break;

		default:
			break;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool Timeline::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
	cr->save();

	//Cache some coordinates
	size_t w = get_width();
	size_t h = get_height();
	double ytop = 2;
	double ybot = h - 10;
	double ymid = (h-10) / 2;

	//Draw the background
	Gdk::Color black("black");
	cr->set_source_rgb(black.get_red_p(), black.get_green_p(), black.get_blue_p());
	cr->rectangle(0, 0, w, h);
	cr->fill();

	//Set the color
	Gdk::Color color("white");
	cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());

	//Draw top line
	cr->move_to(0, ytop);
	cr->line_to(w, ytop);
	cr->stroke();

	//See if we're using time or frequency units
	auto children = m_group->m_waveformBox.get_children();
	enum ScaleMode
	{
		SCALE_TIME,
		SCALE_FREQ
	} scale = SCALE_TIME;
	if(!children.empty())
	{
		auto view = dynamic_cast<WaveformArea*>(children[0]);
		if(view != NULL)
		{
			if(view->IsFFT())
				scale = SCALE_FREQ;
		}
	}

	switch(scale)
	{
		case SCALE_TIME:
			RenderAsTime(cr);
			break;

		case SCALE_FREQ:
			RenderAsFrequency(cr);
			break;
	}

	cr->restore();
	return true;
}

void Timeline::RenderAsTime(const Cairo::RefPtr<Cairo::Context>& cr)
{
	size_t w = get_width();
	size_t h = get_height();
	double ytop = 2;
	double ybot = h - 10;
	double ymid = (h-10) / 2;

	//Figure out what units to use, based on the width of our window
	int64_t width_ps = w / m_group->m_pixelsPerPicosecond;
	const char* units = "ps";
	int64_t unit_divisor = 1;
	int64_t round_divisor = 1;
	string sformat = "%.0lf %s";
	if(width_ps < 1E4)
	{
		//ps, leave default
		if(width_ps < 100)
			round_divisor = 10;
		else if(width_ps < 500)
			round_divisor = 50;
		else if(width_ps < 1000)
			round_divisor = 100;
		else if(width_ps < 2500)
			round_divisor = 250;
		else if(width_ps < 5000)
			round_divisor = 500;
		else
			round_divisor = 1000;

		sformat = "%.0lf %s";
	}
	else if(width_ps < 1E6)
	{
		units = "ns";
		unit_divisor = 1E3;
		round_divisor = 1E3;

		sformat = "%.2lf %s";
	}
	else if(width_ps < 1E9)
	{
		units = "μs";
		unit_divisor = 1E6;

		if(width_ps < 1e8)
			round_divisor = 1e5;
		else
			round_divisor = 1E6;

		sformat = "%.4lf %s";
	}
	else if(width_ps < 1E11)
	{
		units = "ms";
		unit_divisor = 1E9;
		round_divisor = 1E9;

		sformat = "%.6lf %s";
	}
	else
	{
		units = "s";
		unit_divisor = 1E12;
		round_divisor = 1E12;
	}
	//LogDebug("width_ps = %zu, unit_divisor = %zu, round_divisor = %zu\n",
	//	width_ps, unit_divisor, round_divisor);

	//Figure out about how much time per graduation to use
	const int min_label_grad_width = 100;		//Minimum distance between text labels, in pixels
	int64_t grad_ps_nominal = min_label_grad_width / m_group->m_pixelsPerPicosecond;

	//Round so the division sizes are sane
	double units_per_grad = grad_ps_nominal * 1.0 / round_divisor;
	double base = 5;
	double log_units = log(units_per_grad) / log(base);
	double log_units_rounded = ceil(log_units);
	double units_rounded = pow(base, log_units_rounded);
	int64_t grad_ps_rounded = units_rounded * round_divisor;

	//Calculate number of ticks within a division
	double nsubticks = 5;
	double subtick = grad_ps_rounded / nsubticks;

	//Find the start time (rounded down as needed)
	double tstart = floor(m_group->m_timeOffset / grad_ps_rounded) * grad_ps_rounded;

	//Print tick marks and labels
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
	Pango::FontDescription font("sans normal 10");
	font.set_weight(Pango::WEIGHT_NORMAL);
	tlayout->set_font_description(font);
	int swidth;
	int sheight;
	for(double t = tstart; t < (tstart + width_ps + grad_ps_rounded); t += grad_ps_rounded)
	{
		double x = (t - m_group->m_timeOffset) * m_group->m_pixelsPerPicosecond;

		//Draw fine ticks first (even if the labeled graduation doesn't fit)
		for(int tick=1; tick < nsubticks; tick++)
		{
			double subx = (t - m_group->m_timeOffset + tick*subtick) * m_group->m_pixelsPerPicosecond;

			if(subx < 0)
				continue;
			if(subx > w)
				break;

			cr->move_to(subx, ytop);
			cr->line_to(subx, ytop + 10);
		}
		cr->stroke();

		if(x < 0)
			continue;
		if(x > w)
			break;

		//Tick mark
		cr->move_to(x, ytop);
		cr->line_to(x, ybot);
		cr->stroke();

		//Format the string
		double scaled_time = t / unit_divisor;
		char namebuf[256];
		snprintf(namebuf, sizeof(namebuf), sformat.c_str(), scaled_time, units);

		//Render it
		tlayout->set_text(namebuf);
		tlayout->get_pixel_size(swidth, sheight);
		cr->move_to(x+2, ymid + sheight/2);
		tlayout->update_from_cairo_context(cr);
		tlayout->show_in_cairo_context(cr);
	}

	//Draw cursor positions if requested
	Gdk::Color yellow("yellow");
	Gdk::Color orange("orange");

	if( (m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL) ||
		(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_SINGLE) )
	{
		//Dual cursors
		if(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL)
		{
			//Draw filled area between them
			double x = (m_group->m_xCursorPos[0] - m_group->m_timeOffset) * m_group->m_pixelsPerPicosecond;
			double x2 = (m_group->m_xCursorPos[1] - m_group->m_timeOffset) * m_group->m_pixelsPerPicosecond;
			cr->set_source_rgba(yellow.get_red_p(), yellow.get_green_p(), yellow.get_blue_p(), 0.2);
			cr->move_to(x, 0);
			cr->line_to(x2, 0);
			cr->line_to(x2, h);
			cr->line_to(x, h);
			cr->fill();

			//Second cursor
			DrawCursor(
				cr,
				m_group->m_xCursorPos[1],
				"X2",
				orange,
				unit_divisor,
				sformat,
				units,
				false,
				true);
		}

		//First cursor
		DrawCursor(
			cr,
			m_group->m_xCursorPos[0],
			"X1",
			yellow,
			unit_divisor,
			sformat,
			units,
			true,
			false);
	}
}

void Timeline::RenderAsFrequency(const Cairo::RefPtr<Cairo::Context>& cr)
{
	size_t w = get_width();
	size_t h = get_height();
	double ytop = 2;
	double ybot = h - 10;
	double ymid = (h-10) / 2;

	//Figure out what units to use, based on the width of our window
	//Note that scale factor for frequency domain is pixels per Hz, not per ps
	int64_t width_hz = w / m_group->m_pixelsPerPicosecond;
	const char* units = "Hz";
	int64_t unit_divisor = 1;
	int64_t round_divisor = 1;
	string sformat = "%.0lf %s";
	if(width_hz > 1e6)
	{
		units = "MHz";
		unit_divisor = 1E6;
		round_divisor = 1E6;

		sformat = "%.2lf %s";
	}
	else if(width_hz > 1E3)
	{
		units = "kHz";
		unit_divisor = 1e3;
		round_divisor = 10;

		sformat = "%.0lf %s";
	}
	//LogDebug("width_hz = %zu, unit_divisor = %zu, round_divisor = %zu\n",
	//	width_hz, unit_divisor, round_divisor);

	//Figure out about how much time per graduation to use
	const int min_label_grad_width = 100;		//Minimum distance between text labels, in pixels
	int64_t grad_hz_nominal = min_label_grad_width / m_group->m_pixelsPerPicosecond;

	//Round so the division sizes are sane
	double units_per_grad = grad_hz_nominal * 1.0 / round_divisor;
	double base = 5;
	double log_units = log(units_per_grad) / log(base);
	double log_units_rounded = ceil(log_units);
	double units_rounded = pow(base, log_units_rounded);
	int64_t grad_hz_rounded = units_rounded * round_divisor;

	//Calculate number of ticks within a division
	double nsubticks = 5;
	double subtick = grad_hz_rounded / nsubticks;

	//Find the start time (rounded down as needed)
	double tstart = floor(m_group->m_timeOffset / grad_hz_rounded) * grad_hz_rounded;

	//Print tick marks and labels
	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
	Pango::FontDescription font("sans normal 10");
	font.set_weight(Pango::WEIGHT_NORMAL);
	tlayout->set_font_description(font);
	int swidth;
	int sheight;
	for(double t = tstart; t < (tstart + width_hz + grad_hz_rounded); t += grad_hz_rounded)
	{
		double x = (t - m_group->m_timeOffset) * m_group->m_pixelsPerPicosecond;

		//Draw fine ticks first (even if the labeled graduation doesn't fit)
		for(int tick=1; tick < nsubticks; tick++)
		{
			double subx = (t - m_group->m_timeOffset + tick*subtick) * m_group->m_pixelsPerPicosecond;

			if(subx < 0)
				continue;
			if(subx > w)
				break;

			cr->move_to(subx, ytop);
			cr->line_to(subx, ytop + 10);
		}
		cr->stroke();

		if(x < 0)
			continue;
		if(x > w)
			break;

		//Tick mark
		cr->move_to(x, ytop);
		cr->line_to(x, ybot);
		cr->stroke();

		//Format the string
		double scaled_time = t / unit_divisor;
		char namebuf[256];
		snprintf(namebuf, sizeof(namebuf), sformat.c_str(), scaled_time, units);

		//Render it
		tlayout->set_text(namebuf);
		tlayout->get_pixel_size(swidth, sheight);
		cr->move_to(x+2, ymid + sheight/2);
		tlayout->update_from_cairo_context(cr);
		tlayout->show_in_cairo_context(cr);
	}

	//Draw cursor positions if requested
	Gdk::Color yellow("yellow");
	Gdk::Color orange("orange");

	if( (m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL) ||
		(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_SINGLE) )
	{
		//Dual cursors
		if(m_group->m_cursorConfig == WaveformGroup::CURSOR_X_DUAL)
		{
			//Draw filled area between them
			double x = (m_group->m_xCursorPos[0] - m_group->m_timeOffset) * m_group->m_pixelsPerPicosecond;
			double x2 = (m_group->m_xCursorPos[1] - m_group->m_timeOffset) * m_group->m_pixelsPerPicosecond;
			cr->set_source_rgba(yellow.get_red_p(), yellow.get_green_p(), yellow.get_blue_p(), 0.2);
			cr->move_to(x, 0);
			cr->line_to(x2, 0);
			cr->line_to(x2, h);
			cr->line_to(x, h);
			cr->fill();

			//Second cursor
			DrawCursor(
				cr,
				m_group->m_xCursorPos[1],
				"X2",
				orange,
				unit_divisor,
				sformat,
				units,
				false,
				true,
				true);
		}

		//First cursor
		DrawCursor(
			cr,
			m_group->m_xCursorPos[0],
			"X1",
			yellow,
			unit_divisor,
			sformat,
			units,
			true,
			false,
			true);
	}
}

void Timeline::DrawCursor(
	const Cairo::RefPtr<Cairo::Context>& cr,
	int64_t ps,
	const char* name,
	Gdk::Color color,
	double unit_divisor,
	string sformat,
	const char* units,
	bool draw_left,
	bool show_delta,
	bool is_frequency)
{
	int h = get_height();

	Gdk::Color black("black");

	Glib::RefPtr<Pango::Layout> tlayout = Pango::Layout::create (cr);
	Pango::FontDescription font("sans normal 10");
	font.set_weight(Pango::WEIGHT_NORMAL);
	tlayout->set_font_description(font);
	int swidth;
	int sheight;

	//Format label for cursor
	char label[256];
	if(!show_delta)
	{
		string format("%s: ");
		format += sformat;
		snprintf(
			label,
			sizeof(label),
			format.c_str(),
			name,
			ps / unit_divisor,
			units);
	}
	else
	{
		string format("%s: ");
		format += sformat;
		format += "\nΔX = ";
		format += sformat;
		if(!is_frequency)
			format += " (%.3f MHz)\n";
		int64_t dt = m_group->m_xCursorPos[1] - m_group->m_xCursorPos[0];
		double delta = dt / unit_divisor;
		double mhz = 1.0e6 / dt;
		snprintf(
			label,
			sizeof(label),
			format.c_str(),
			name,
			ps / unit_divisor,
			units,
			delta,
			units,
			mhz);
	}
	tlayout->set_text(label);
	tlayout->get_pixel_size(swidth, sheight);

	//Decide which side of the line to draw on
	double x = (ps - m_group->m_timeOffset) * m_group->m_pixelsPerPicosecond;
	double right = x-5;
	double left = right - swidth - 5;
	if(!draw_left)
	{
		left = x + 5;
		right = left + swidth + 5;
	}

	//Draw filled background for label
	cr->set_source_rgba(black.get_red_p(), black.get_green_p(), black.get_blue_p(), 0.75);
	double bot = 10;
	double top = bot + sheight;
	cr->move_to(left, top);
	cr->line_to(left, bot);
	cr->line_to(right, bot);
	cr->line_to(right, top);
	cr->fill();

	//Label text
	cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());
	cr->move_to(left+5, bot);
	tlayout->update_from_cairo_context(cr);
	tlayout->show_in_cairo_context(cr);

	//Cursor line
	cr->move_to(x, 0);
	cr->line_to(x, h);
	cr->set_source_rgb(color.get_red_p(), color.get_green_p(), color.get_blue_p());
	cr->stroke();
}
