
/*
    Copyright 2009, Romain Behar <romainbehar@users.sourceforge.net>

    This file is part of Shrimp 2.

    Shrimp 2 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Shrimp 2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Shrimp 2.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "ui_scene_view.h"

#include "ui_add_input.h"
#include "ui_add_output.h"
#include "ui_block_info.h"
#include "ui_edit_code.h"
#include "ui_edit_block_name.h"
#include "ui_edit_group_name.h"
#include "ui_edit_input.h"
#include "ui_edit_output.h"
#include "ui_edit_rib.h"

#include "../miscellaneous/logging.h"
#include "../miscellaneous/misc_string_functions.h"


#include <fltk/ask.h>
#include <fltk/events.h>
#include <fltk/gl.h>
#include <fltk/Item.h>
#include <fltk/MenuBar.h>
#include <fltk/PopupMenu.h>

#if defined(__APPLE__) && defined (__MACH__)
    #include <OpenGL/glu.h>
#else
    #include <GL/glu.h>
#endif

#include <iostream>
#include <cmath>
#include <limits>


bool point_between (double a, double b, double c)
{
	double lb = a <= b;
	double ub = b <= c;
	double ans = lb && ub;
// printf("a %f\n",a);
// printf("b %f\n",b);
// printf("c %f\n",c);
// printf("point_between %f\n", ans);
	return ans;
}

bool point_inside(double x,double y,double XX, double YY, double WW,double HH)
{
	bool in_lr = point_between (XX, x, WW);
	bool in_tb = point_between (YY, y, HH);
	bool inside = in_lr && in_tb;
// printf("point_inside %d\n",inside);
	return inside;
}



opengl_view::opengl_view (services* services_instance) :
	m_services (services_instance),
	m_console (0),
	m_min_block_height (0.5),
	m_size (3),
	m_last_mouse_x (0),
	m_last_mouse_y (0),
	m_mouse_click (0),
	m_projection_left (-10),
	m_projection_right (10),
	m_projection_bottom (-10),
	m_projection_top (10),
	m_projection_near (-1000),
	m_projection_far (1000),

	m_grid (false),
	m_snap_to_grid (false),
	m_overview (false),
	m_font_size (10)
{

}


void opengl_view::set_size (const double Size)
{
	m_size = Size;

	// set min and max
	if (m_size < 0.1) {
		m_size = 0.1;
	}
	if (m_size > 5) {
		m_size = 5;
	}
}


double opengl_view::fit_scene(int window_width, int window_height)
{
	// make sure the projection is correct since below computations are based on it
	update_projection(window_width, window_height);

	// compute shaders' bounding-box
	double left = 0;
	double right = 0;
	double bottom = 0;
	double top = 0;
	shader_block_map_t block_list = m_services->get_scene_blocks();
	for (shader_block_map_t::const_iterator block_i = block_list.begin(); block_i != block_list.end(); ++block_i) {
		shader_block* block = block_i->second;

		// the first block serves as reference
		if (block_list.begin() == block_i) {

			left = block->m_position_x;
			right = block->m_position_x + block->m_width;
			bottom = block->m_position_y;
			top = block->m_position_y - block->m_height;
		} else {
			left = std::min (left, block->m_position_x);
			right = std::max (right, block->m_position_x + block->m_width);
			bottom = std::min (bottom, block->m_position_y);
			top = std::max (top, block->m_position_y - block->m_height);
		}
	}

	// get ratios
	const double scene_ratio = (right - left) / (top - bottom);
	const double view_ratio = static_cast<double> (m_view_width) / static_cast<double> (m_view_height);

	if (std::fabs (scene_ratio) > std::fabs (view_ratio)) {
		// fit the width
		const double width = m_projection_right - m_projection_left;
		set_size (width / (1.2 * std::fabs (right - left)));
	} else {
		// fit the height
		const double height = m_projection_top - m_projection_bottom;
		set_size (height / (1.2 * std::fabs (bottom - top)));
	}

	// resize the bounding box position to the scene size
	const double center_x = (left + right) / 2 * m_size;
	const double center_y = (top + bottom) / 2 * m_size;
	// move to the scene centre
	center_scene (center_x, center_y);

	return m_size;
}

void opengl_view::center_scene (const double X, const double Y)
{
	const double width = m_projection_right - m_projection_left;
	const double height = m_projection_top - m_projection_bottom;

	m_projection_left = X - width / 2;
	m_projection_right = X + width / 2;
	m_projection_bottom = Y - height / 2;
	m_projection_top = Y + height / 2;
}


void opengl_view::move_active_block (const double XOffset, const double YOffset)
{
	int group_total = m_services->group_selection_size();
	int total = m_services->selection_size() + group_total;
	shader_block* block = m_services->get_block (m_active_block);


	//If multi selecion
	if (total>1){

		services::block_selection_t& selection = m_services->get_block_selection();
		for (services::block_selection_t::const_iterator block_i = selection.begin(); block_i != selection.end(); ++block_i) {

			std::string current_selection = *block_i;

			shader_block* block = m_services->get_block(current_selection);
			int group = m_services->group (block);

			if (!block && !group) {

					log() << error << "active block or group'" << current_selection << "' not found." << std::endl;
					return;
				}

			if (!group){
				block->m_position_x += XOffset;
				block->m_position_y += YOffset;
				}
			else move_active_group(XOffset,YOffset);
		}
	}

	//If single selection no parsing grab m_active_block
	else if (block && (total<2)){
			block->m_position_x += XOffset;
			block->m_position_y += YOffset;
		   }
	//Move group as well
	if (group_total || m_active_group){
	move_active_group(XOffset,YOffset);
	}
	else if (!block) {
						log() << error << "active block FFFF'" << m_active_block << "' not found." << std::endl;
						return;
					}
}

void opengl_view::move_all_blocks (const double XOffset, const double YOffset) {

	shader_block_map_t block_list = m_services->get_scene_blocks();
	for (scene::shader_blocks_t::iterator block = block_list.begin(); block != block_list.end(); ++block) {

		block->second->m_position_x += XOffset;
		block->second->m_position_y += YOffset;
	}
}


void opengl_view::move_block_to_view_center (shader_block* Block) {

	if (!Block) {

		log() << error << "no block supplied! (move_block_to_view_center)" << std::endl;
		return;
	}

	const double center_x = (m_projection_right + m_projection_left) / 2 / m_size;
	const double center_y = (m_projection_bottom + m_projection_top) / 2 / m_size;

	Block->m_position_x = center_x;
	Block->m_position_y = center_y;
}


void opengl_view::move_scene (const double XOffset, const double YOffset) {

	m_projection_left -= XOffset;
	m_projection_right -= XOffset;
	m_projection_bottom -= YOffset;
	m_projection_top -= YOffset;
}


void opengl_view::move_active_group (const double XOffset, const double YOffset)
{
	int total = m_services->group_selection_size();

	if (!total && (m_active_group == 0)) {

		log() << error << "there's no group to move!" << std::endl;
		return;
	}

	// move all the group's shaders
	for (shrimp::groups_t::const_iterator g = m_services->groups().begin(); g != m_services->groups().end(); ++g) {

		shader_block* block = m_services->get_block (g->first);
		//Only one group
		if (g->second == m_active_group) {

			block->m_position_x += XOffset;
			block->m_position_y += YOffset;
		}
		//All selected group
		else if (m_services->is_group_selected(g->second)){

			block->m_position_x += XOffset;
			block->m_position_y += YOffset;
		}

	}
}

//copy blocks
void opengl_view::copy_selected_blocks()
{

	int total = m_services->selection_size();
	int group_total = m_services->group_selection_size();
	shader_block* block = m_current_selection_block;
	//TODO:
	//m_services->clear_copy_selection();
	//m_services->m_copy_buffer.clear();

	if (!block){
		log() << error << "active block '" << m_current_selection_block << "' not found." << std::endl;
		return;
	}

	std::string block_name;

	//copy block
	//If multi seletcion
		if (total>1) {

			services::block_selection_t& block_list = m_services->get_block_selection();
			for (services::block_selection_t::const_iterator block_i = block_list.begin(); block_i != block_list.end(); ++block_i) {

				std::string current_selection = *block_i;

				shader_block* block_copy = m_services->get_block(current_selection);
				block_name = block_copy->name();

				if (!block_copy) {

						log() << error << "active block '" << current_selection << "' not found." << std::endl;
						return;
					}

				int group = m_services->group (block_copy);
				if (!group)
				{
					//TODO:
					//m_services->copy_blocks (block_name,0);
				}
			}
		}

		//If single selection no parsing grab m_active_block
		else if (!block)
		{
			log() << error << "active block '" << m_active_block << "' not found." << std::endl;
			return;
		}

		else if (block && (total<2)){
			block_name = block->name();
			//TODO:
			//m_services->copy_blocks(block_name,0);

			   }
	//copy group as well

	if (group_total>=1){
		//TODO:
		//copy_selected_groups();
	}
}

//copy groups
void opengl_view::copy_selected_groups()
{
/*
	for (scene::groups_selection_t::const_iterator g = m_services->m_groups_selection.begin(); g != m_services->m_groups_selection.end(); ++g)
		{
			for (scene::shader_blocks_t::iterator block = m_services->m_blocks.begin(); block != m_services->m_blocks.end(); ++block)
				{
					const int group_org = *g;
					if (group_org==m_services->group(block->second) )
					{

						m_services->copy_blocks(block->first,group_org);
					}
				}

		}
*/
}

void opengl_view::paste_buffered_blocks()
{
//paste blocks
/*
	m_services->clear_selection();

	if (m_services->m_copy_selection.size()){
	m_services->copy_connections();
	m_services->paste_blocks();

	// make block copy current selection
	for (scene::shader_blocks_copy_t::iterator new_block = m_services->m_copy_buffer.begin(); new_block != m_services->m_copy_buffer.end(); ++new_block){

			// Don't select block part of a group
			if (!m_services->group(new_block->second.second))
					{
					m_services->set_block_selection(new_block->second.second,1);
					}
		}
	// make group copy current selection
	for (scene::groups_t::const_iterator g = m_services->m_groups_buffer.begin(); g != m_services->m_groups_buffer.end(); ++g) {
			m_services->set_group_selection(g->second,1);
		}

	}
	m_services->clear_copy_selection();
*/
}

void opengl_view::box_selection(int window_width, int window_height)
{

	//2D opengl drawing
	glShadeModel (GL_FLAT);
	glDisable (GL_LINE_SMOOTH);
	glDisable (GL_BLEND);
	glLineWidth (1.0);

	glColor3f (0.8, 0.0, 0.0);
	glLineStipple(3, 0xAAAA);
	glEnable(GL_LINE_STIPPLE);
	glPushMatrix();
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D (0, static_cast<float> (window_width), 0, static_cast<float> (window_height));


	//Mouse marquee position
	double from_x = static_cast<float> (m_start_drag_x);
	double from_y = static_cast<float> (window_height) - static_cast<float> (m_start_drag_y);
	double to_x = static_cast<float> (m_current_mouse_x);
	double to_y = static_cast<float> (window_height) - static_cast<float> (m_current_mouse_y);


	//Rectangle selection stipple
	glBegin(GL_LINE_LOOP);
		glVertex3d(from_x, from_y,0);
		glVertex3d(from_x,to_y,0);
		glVertex3d(to_x,to_y,0);
		glVertex3d(to_x,from_y,0);


	glEnd();

	glPopMatrix();
	glDisable(GL_LINE_STIPPLE);


	//Detection of block selected
	std::map<unsigned long, const shader_block*> block_indices;
			unsigned long index = 1;

	shader_block_map_t block_list = m_services->get_scene_blocks();
	for (scene::shader_blocks_t::const_iterator block_i = block_list.begin(); block_i != block_list.end(); ++block_i)
	{
		const shader_block* block = block_i->second;
		shader_block* blockSel = block_i->second;

		//Project rectangle selection in "Block" space
		GLdouble Fx,Fy,Fz;
		GLdouble Tx,Ty,Tz;

		GLint viewport[4];
		GLdouble mvmatrix[16], projmatrix[16];
		glGetDoublev(GL_MODELVIEW_MATRIX,mvmatrix );
		glGetDoublev(GL_PROJECTION_MATRIX,projmatrix );
		glGetIntegerv(GL_VIEWPORT,viewport );

	    //Mouse selection direction
	    //Fx<Tx : Ty<Fy
	    //Fx>Tx : Ty<Fy
	    //Fx>Tx : Ty>Fy
	    //Fx<Tx : Ty>Fy

		gluUnProject(from_x,from_y,0, mvmatrix, projmatrix,viewport,&Fx,&Fy,&Fz );
		gluUnProject(to_x,to_y,0, mvmatrix, projmatrix,viewport,&Tx,&Ty,&Tz );

		const int group = m_services->group (block);
		if (!group) {
			//Height of the block
			const double width = blockSel->m_width;

			const unsigned long max_properties = std::max (blockSel->input_count(), (unsigned long)blockSel->m_outputs.size()); // cast required by some unusual compilers (e.g. gcc version 4.1.3 20070929 (prerelease))

			// set minimal block height
			const double height1 = m_services->is_rolled (blockSel) ? width : (width * (1.0 / 3.7) * static_cast<double> (max_properties));
			const double height = (height1 < m_min_block_height) ? m_min_block_height : height1;

			//Above block
		    if (Fx<Tx && Ty<Fy){
		    //Check if rectangle surround center of the block
			if (point_inside (blockSel->m_position_x+width/2, blockSel->m_position_y-height/2 ,Fx,Ty,Tx,Fy))
					{
				//Make Block selected
				m_services->set_block_selection (blockSel, true);
				m_current_selection_block = blockSel;
					}
			else m_services->set_block_selection (blockSel, false);
		    }
		    else if (Fx>Tx && Ty<Fy){
			//Check if rectangle surround center of the block
				if (point_inside (blockSel->m_position_x+width/2 ,blockSel->m_position_y-height/2 ,Tx,Ty,Fx,Fy))
					{
					//Make Block selected
					m_services->set_block_selection (blockSel, true);
					m_current_selection_block = blockSel;
					}
				else m_services->set_block_selection (blockSel, false);
			}
		   else if (Fx>Tx && Ty>Fy){
					//Check if rectangle surround center of the block
				if (point_inside (blockSel->m_position_x+width/2 ,blockSel->m_position_y-height/2 ,Tx,Fy,Fx,Ty))
					{
					//Make Block selected
					m_services->set_block_selection (blockSel, true);
					m_current_selection_block = blockSel;
					}
				else m_services->set_block_selection (blockSel, false);
				}
		  else if (Fx<Tx && Ty>Fy){
					//Check if rectangle surround center of the block
				if (point_inside (blockSel->m_position_x+width/2 ,blockSel->m_position_y-height/2 ,Fx,Fy,Tx,Ty))
					{
					//Make Block selected
					m_services->set_block_selection (blockSel, true);
					m_current_selection_block = blockSel;
					}
				else m_services->set_block_selection (blockSel, false);
				}


			glLoadName (index);

			block_indices.insert (std::make_pair (index, blockSel));

			++index;
		}

		if (group) {


			group_position_t::const_iterator p = m_group_positions.find(group);

			//Get group position
			const double x = p->second.position_x;
			const double y = p->second.position_y;

			 //Above group
			   //Check if rectangle surround center of the group
			   if (Fx<Tx && Ty<Fy){
				//Check if rectangle surround center of the group
					if (point_inside (x ,y ,Fx,Ty,Tx,Fy))
						{
						//Make group selected
						m_services->set_group_selection (group, true);
						m_current_group = p->first;
						}
					else m_services->set_group_selection (group, false);
				}
				else if (Fx>Tx && Ty<Fy){
					//Check if rectangle surround center of the group
					if (point_inside (x,y ,Tx,Ty,Fx,Fy))
						{
						//Make group selected
						m_services->set_group_selection (group, true);
						m_current_group = p->first;
						}
					else m_services->set_group_selection (group, false);
					}
			   else if (Fx>Tx && Ty>Fy){
						//Check if rectangle surround center of the group
					if (point_inside (x ,y ,Tx,Fy,Fx,Ty))
						{
						//Make group selected
						m_services->set_group_selection (group, true);
						m_current_group = p->first;
						}
					else m_services->set_group_selection (group, false);
					}
			  else if (Fx<Tx && Ty>Fy){
						//Check if rectangle surround center of the group
					if (point_inside (x ,y ,Fx,Fy,Tx,Ty))
						{
						//Make group selected
						m_services->set_group_selection (group, true);
						m_current_group = p->first;
						}
					else m_services->set_group_selection (group, false);
					}

				glLoadName (index);

				block_indices.insert (std::make_pair (index, blockSel));

				++index;
		}
	}

}
void opengl_view::draw_grid() {

	glShadeModel (GL_FLAT);
	glDisable (GL_LINE_SMOOTH);
	glDisable (GL_BLEND);
	glLineWidth (1.0);

	glColor3f (0.3, 0.3, 0.3);

	// grid's horizontal lines
	glBegin (GL_LINES);
		for (double i = -100; i < 100; ++i) {

			glVertex3d (-100, i, 0);
			glVertex3d (100, i, 0);
		}
	glEnd();

	// grid's vertical lines
	glBegin (GL_LINES);
		for (double i = -100; i < 100; ++i) {

			glVertex3d (i, -100, 0);
			glVertex3d (i, 100, 0);
		}
	glEnd();
}


void opengl_view::draw_scene (bool valid, int window_width, int window_height) {

	// draw scene
	glMatrixMode (GL_PROJECTION);

	if (!valid) {

		update_projection(window_width, window_height);
	}

	glLoadIdentity();
	glViewport (0, 0, m_view_width, m_view_height);

	glOrtho (m_projection_left, m_projection_right, m_projection_bottom, m_projection_top, m_projection_near, m_projection_far);

	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// background color
	glClearColor( 0.2f, 0.2f, 0.2f, 1);

	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();
		transform_scene();

		if (m_grid) {
			draw_grid();
		}

		draw_shader(window_width, window_height);
		if (m_box_selection)
		{
			box_selection(window_width, window_height);
		}

	glPopMatrix();

	// console (if any)
	draw_console(window_width, window_height);
}


void opengl_view::draw_shader(int window_width, int window_height) {

	glShadeModel (GL_FLAT);
	glEnable (GL_LINE_SMOOTH);
	glEnable (GL_BLEND);
	glHint (GL_LINE_SMOOTH_HINT, GL_NICEST);
	glLineWidth (1.5);

	// initialize group positions
	m_group_positions.clear();

	typedef std::map<int, int> group_counts_t;
	group_counts_t counts;
	for (shrimp::groups_t::const_iterator g = m_services->groups().begin();
		g != m_services->groups().end(); ++g) {

		const std::string block_name = g->first;
		const int group_number = g->second;

		shader_block* block = m_services->get_block (block_name);

		// count current group
		group_counts_t::iterator c = counts.find (group_number);
		if (c == counts.end())
			counts.insert (std::make_pair (group_number, 1));
		else
			c->second++;

		// store position
		group_position_t::iterator p = m_group_positions.find (group_number);
		if (p == m_group_positions.end()) {
			m_group_positions.insert (std::make_pair (group_number, position (block->m_position_x, block->m_position_y)));
		} else {
			p->second.position_x += block->m_position_x;
			p->second.position_y += block->m_position_y;
		}
	}

	for (group_position_t::iterator p = m_group_positions.begin(); p != m_group_positions.end(); ++p) {
		group_counts_t::iterator c = counts.find (p->first);
		if (c == counts.end()) {
			log() << error << "Should not be reached." << std::endl;
			continue;
		}

		// compute the average position
		p->second.position_x /= static_cast<double> (c->second);
		p->second.position_y /= static_cast<double> (c->second);
	}

	// draw blocks
	positions_t property_positions;
	shader_block_map_t block_list = m_services->get_scene_blocks();
	for (scene::shader_blocks_t::const_iterator block_i = block_list.begin(); block_i != block_list.end(); ++block_i) {

		const shader_block* block = block_i->second;

		// draw blocks that don't belong to any group
		int group = m_services->group (block);
		if (!group) {
			if (m_snap_to_grid && (block->name() == m_active_block)) {

				double x = block->m_position_x;
				double y = block->m_position_y;

				snap_position (x, y);

				draw_block (block, x, y, property_positions);
			} else {

				draw_block (block, block->m_position_x, block->m_position_y, property_positions);
			}
		}
	}

	// draw connections
	glColor3f (0.8, 0.4, 0.4);
		shrimp::dag_t scene_dag = m_services->get_scene_dag();
		for (shrimp::dag_t::const_iterator connection = scene_dag.begin(); connection != scene_dag.end(); ++connection) {

			const shrimp::io_t to = connection->first;
			const shrimp::io_t from = connection->second;

			double to_x = 0;
			double to_y = 0;
			double from_x = 0;
			double from_y = 0;

			const positions_t::const_iterator to_property = property_positions.find (to);
			if (property_positions.end() == to_property) {

				// if the property has a parent, get its position
				const shader_block* block = m_services->get_block (to.first);
				const std::string parent = block->get_input_parent (to.second);
				if (!parent.empty()) {

					// get the parent property's position
					const positions_t::const_iterator parent_property = property_positions.find (shrimp::io_t (to.first, parent));
					if (property_positions.end() == parent_property) {

						log() << error << "parent property '" << parent << "' not found in block '" << to.first << "'" << std::endl;
					} else {
						to_x = parent_property->second.position_x;
						to_y = parent_property->second.position_y;
					}

				} else {

					// property not found, may be part of a group or rolled block
					const int block_group = m_services->group (block);
					const bool rolled_block = m_services->is_rolled (block);

					if (block_group) {

						// set group's position
						group_position_t::const_iterator p = m_group_positions.find (block_group);
						if (p == m_group_positions.end()) {
							log() << error << "group '" << block_group << "' not found." << std::endl;
							continue;
						}

						to_x = p->second.position_x;
						to_y = p->second.position_y;

					} else if (rolled_block) {

						to_x = block->m_position_x + block->m_width / 2;
						to_y = block->m_position_y - block->m_width / 2;

					} else {

						log() << error << "start property '" << to.first << "-" << to.second << "' not found." << std::endl;
						continue;
					}

				}
			}
			else {
				to_x = to_property->second.position_x;
				to_y = to_property->second.position_y;
			}

			const positions_t::const_iterator from_property = property_positions.find (from);
			if (property_positions.end() == from_property) {

				const shader_block* block = m_services->get_block (from.first);

				// property not found, may be part of a group or rolled block
				const int block_group = m_services->group (block);
				const bool rolled_block = m_services->is_rolled (block);

				if (block_group) {

					// set group's position
					group_position_t::const_iterator p = m_group_positions.find (block_group);
					if (p == m_group_positions.end()) {
						log() << error << "group '" << block_group << "' not found." << std::endl;
						continue;
					}

					from_x = p->second.position_x;
					from_y = p->second.position_y;

				} else if (rolled_block) {

					from_x = block->m_position_x + block->m_width / 2;
					from_y = block->m_position_y - block->m_width / 2;

				} else {

					log() << error << "end property '" << from.first << "-" << from.second << "' not found." << std::endl;
					continue;
				}
			}
			else
			{
				from_x = from_property->second.position_x;
				from_y = from_property->second.position_y;
			}

//			// draw a line between property centers
//			glVertex3d (to_x, to_y, 0);
//			glVertex3d (from_x, from_y, 0);
			// draw a spline between property centers
			GLfloat ctrlpoints[6][3] = {
				{ from_x+0.1,from_y,0.0}, {from_x+0.25,from_y,0.0},{from_x+1.0,from_y,0.0},
				{ to_x-1.1,to_y,0.0}, {to_x-0.35,to_y,0.0}, {to_x-0.1,to_y,0.0}

			};
			glMap1f(GL_MAP1_VERTEX_3, 0.0, 1.0, 3, 6, &ctrlpoints[0][0]);
			glEnable(GL_MAP1_VERTEX_3);
			glBegin(GL_LINE_STRIP);
			for (int i = 0; i <= 30; i ++) {
				glEvalCoord1f((GLfloat) i/30.0);
			}
			glEnd();
			glDisable(GL_MAP1_VERTEX_3);

			// draw arrow heads
			glBegin(GL_TRIANGLES);
			glVertex3f( to_x-0.1, to_y, 0.0);
			glVertex3f( to_x-0.18, to_y+0.08, 0.0);
			glVertex3f( to_x-0.18, to_y-0.08, 0.0);
			glVertex3f( to_x-0.1, to_y, 0.0);
			glEnd();
		}

	// draw groups
	draw_groups();

	// draw connection in progress
	if (m_connection_start.first != "")
	{
		const positions_t::const_iterator start_property = property_positions.find (m_connection_start);
		if (property_positions.end() == start_property) {

			log() << error << "connection start property '" << m_connection_start.first << "-" << m_connection_start.second << "' not found." << std::endl;
		}
		else {
			// draw a line between the connection start and mouse pointer
			glColor3f (0.8, 0.0, 0.0);
			glLineStipple(3, 0xAAAA);
			glEnable(GL_LINE_STIPPLE);
			glPushMatrix();
			glMatrixMode (GL_PROJECTION);
			glLoadIdentity();
			gluOrtho2D (0, static_cast<float> (window_width), 0, static_cast<float> (window_height));

			// draw a spline between property centers
			double to_x = static_cast<float> (m_current_mouse_x);
			double to_y = static_cast<float> (window_height) - static_cast<float> (m_current_mouse_y);
			double from_x = static_cast<float> (m_connection_start_x);
			double from_y = static_cast<float> (window_height) - static_cast<float> (m_connection_start_y);


			GLfloat ctrlpoints[6][3] = {
				{ from_x,from_y,0.0}, {from_x+25,from_y,0.0},{from_x+100,from_y,0.0},
				{ to_x-110,to_y,0.0}, {to_x-35,to_y,0.0}, {to_x,to_y,0.0}
			};
			glMap1f(GL_MAP1_VERTEX_3, 0.0, 1.0, 3, 6, &ctrlpoints[0][0]);
			glEnable(GL_MAP1_VERTEX_3);
			glBegin(GL_LINE_STRIP);

			for (int i = 0; i <= 30; i ++) {
				glEvalCoord1f((GLfloat) i/30.0);
			}

			glEnd();

			glPopMatrix();
			glDisable(GL_MAP1_VERTEX_3);
			glDisable(GL_LINE_STIPPLE);
		}
	}
}

void opengl_view::update_projection(int window_width, int window_height) {

	m_view_width = window_width;
	m_view_height = window_height;

	const double ratio = static_cast<double> (m_view_width) / static_cast<double> (m_view_height);
	const double centre_x = (m_projection_left + m_projection_right) / 2;
	const double centre_y = (m_projection_bottom + m_projection_top) / 2;

	if (ratio > 1) {
		// size : 100 pixels per scene unit
		const double size = static_cast<double> (m_view_height) / 100;
		m_projection_left = -ratio * size;
		m_projection_right = ratio * size;
		m_projection_bottom = -size;
		m_projection_top = size;
	} else {
		// size : 100 pixels per scene unit
		const double size = static_cast<double> (m_view_width) / 100;
		m_projection_left = -size;
		m_projection_right = size;
		m_projection_bottom = -1/ratio * size;
		m_projection_top = 1/ratio * size;
	}

	m_projection_left += centre_x;
	m_projection_right += centre_x;
	m_projection_bottom += centre_y;
	m_projection_top += centre_y;
}


std::string opengl_view::select_object() {
	log() << aspect << "opengl_view: select_object" << std::endl;

	// get current viewport
	GLint viewport[4];
	glGetIntegerv (GL_VIEWPORT, viewport);

	// setup OpenGL selection
	const GLsizei buffer_size = 1024;
	GLuint selection_buffer[buffer_size];
	glSelectBuffer (buffer_size, selection_buffer);
	glRenderMode (GL_SELECT);

	glInitNames();
	// push default name, it will be replaced using glLoadName()
	glPushName (0);

	glMatrixMode (GL_PROJECTION);
	glPushMatrix();
		glLoadIdentity();
		gluPickMatrix ((GLdouble)fltk::event_x(), (GLdouble) (viewport[3] - fltk::event_y()), 1, 1, viewport);
		glOrtho (m_projection_left, m_projection_right, m_projection_bottom, m_projection_top, m_projection_near, m_projection_far);

		transform_scene();

		std::map<unsigned long, const shader_block*> block_indices;
		unsigned long index = 1;
		shader_block_map_t block_list = m_services->get_scene_blocks();
		for (scene::shader_blocks_t::const_iterator block_i = block_list.begin(); block_i != block_list.end(); ++block_i) {

			const shader_block* block = block_i->second;

			int group = m_services->group (block);
			if (!group) {

				glLoadName (index);

				draw_block_body (block, block->m_position_x, block->m_position_y);

				block_indices.insert (std::make_pair (index, block));

				++index;
			}
		}

	glMatrixMode (GL_PROJECTION);
	glPopName();
	glPopMatrix();
	glFlush();

	// Get list of picked blocks
	GLint hits = glRenderMode (GL_RENDER);
	if (hits <= 0)
		return "";

	GLuint closest = 0;
	GLuint dist = 0xFFFFFFFFU;
	while (hits) {

		if (selection_buffer[ (hits - 1) * 4 + 1] < dist) {

			dist = selection_buffer[ (hits - 1) * 4 + 1];
			closest = selection_buffer[ (hits - 1) * 4 + 3];
		}

		hits--;
	}

	return block_indices[closest]->name();
}

shrimp::io_t opengl_view::select_property() {

	// get current viewport
	GLint viewport[4];
	glGetIntegerv (GL_VIEWPORT, viewport);

	// setup OpenGL selection
	const GLsizei buffer_size = 1024;
	GLuint selection_buffer[buffer_size];
	glSelectBuffer (buffer_size, selection_buffer);
	glRenderMode (GL_SELECT);

	glInitNames();
	// push default name, it will be replaced using glLoadName()
	glPushName (0);

	glMatrixMode (GL_PROJECTION);
	glPushMatrix();
		glLoadIdentity();
		gluPickMatrix ((GLdouble)fltk::event_x(), (GLdouble) (viewport[3] - fltk::event_y()), 1, 1, viewport);
		glOrtho (m_projection_left, m_projection_right, m_projection_bottom, m_projection_top, m_projection_near, m_projection_far);

		transform_scene();

		m_property_indices.clear();
		m_property_index = 1;
		shader_block_map_t block_list = m_services->get_scene_blocks();
		for (scene::shader_blocks_t::const_iterator block_i = block_list.begin(); block_i != block_list.end(); ++block_i) {

			const shader_block* block = block_i->second;

			int group = m_services->group (block);
			if (!group) {
				positions_t property_positions;
				draw_block_properties (block, block->m_position_x, block->m_position_y, property_positions, true);
			}
		}

	glMatrixMode (GL_PROJECTION);
	glPopName();
	glPopMatrix();
	glFlush();

	// Get list of picked blocks
	GLint hits = glRenderMode (GL_RENDER);
	if (hits <= 0)
		return std::make_pair ("", "");

	GLuint closest = 0;
	GLuint dist = 0xFFFFFFFFU;
	while (hits) {

		if (selection_buffer[ (hits - 1) * 4 + 1] < dist) {

			dist = selection_buffer[ (hits - 1) * 4 + 1];
			closest = selection_buffer[ (hits - 1) * 4 + 3];
		}

		hits--;
	}

	const shader_block* selected_block = m_property_indices[closest].first;
	if (selected_block)
		return std::make_pair (m_property_indices[closest].first->name(), m_property_indices[closest].second);

	return std::make_pair ("", "");
}

int opengl_view::select_group() {

	// get group list
	shrimp::group_set_t groups = m_services->group_list();

	// Get current viewport
	GLint viewport[4];
	glGetIntegerv (GL_VIEWPORT, viewport);

	// Setup OpenGL selection
	const GLsizei buffer_size = 1024;
	GLuint selection_buffer[buffer_size];
	glSelectBuffer (buffer_size, selection_buffer);
	glRenderMode (GL_SELECT);

	glInitNames();
	// Push default name, it will be replaced using glLoadName()
	glPushName (0);

	glMatrixMode (GL_PROJECTION);
	glPushMatrix();
		glLoadIdentity();
		gluPickMatrix ((GLdouble)fltk::event_x(), (GLdouble) (viewport[3] - fltk::event_y()), 1, 1, viewport);
		glOrtho (m_projection_left, m_projection_right, m_projection_bottom, m_projection_top, m_projection_near, m_projection_far);

		transform_scene();

		for (shrimp::group_set_t::const_iterator g = groups.begin(); g != groups.end(); ++g) {

			glLoadName (*g);
			draw_group_body (*g);
		}

	glMatrixMode (GL_PROJECTION);
	glPopName();
	glPopMatrix();
	glFlush();

	// Get list of picked blocks
	GLint hits = glRenderMode (GL_RENDER);
	if (hits <= 0)
		return 0;

	GLuint closest = 0;
	GLuint dist = 0xFFFFFFFFU;
	while (hits) {

		if (selection_buffer[ (hits - 1) * 4 + 1] < dist) {

			dist = selection_buffer[ (hits - 1) * 4 + 1];
			closest = selection_buffer[ (hits - 1) * 4 + 3];
		}

		hits--;
	}

	return closest;
}


void opengl_view::transform_scene() {

	glScaled (m_size, m_size, m_size);
}

void opengl_view::draw_block (const shader_block* Block, const double X, const double Y, positions_t& PropertyPositions)
{
	if (!m_services->is_rolled (Block))
	{
		draw_block_body (Block, X, Y);
		draw_block_name (Block, X, Y);
		draw_block_properties (Block, X, Y, PropertyPositions);
	}
	else
	{
		draw_rolled_block_body (Block, X, Y);
		draw_block_name (Block, X, Y);
	}
}


void opengl_view::draw_block_body (const shader_block* Block, const double X, const double Y)
{
	const double width = Block->m_width;
	const unsigned long max_properties = std::max (Block->input_count(), (unsigned long)Block->m_outputs.size()); // cast required by some unusual compilers (e.g. gcc version 4.1.3 20070929 (prerelease))

	// set minimal block height
	const double height1 = m_services->is_rolled (Block) ? width : (width * (1.0 / 3.7) * static_cast<double> (max_properties));
	const double height = (height1 < m_min_block_height) ? m_min_block_height : height1;

	const double alpha = 0.5;

	// check whether the block's selected
	const bool is_selected = m_services->is_selected (Block);
	// block color
	if (is_selected)
		// selected blocks are "hover orange"
		glColor4f (1.0, 0.55, 0.0, alpha);
	else if (!Block->m_inputs.size())
		// inputs are green
		glColor4f (0.0, 1.0, 0.0, alpha);
	else
		// other ones are blue
		glColor4f (0.33, 0.47, 0.69, alpha);

	const float radius = 0.1;
	const float step = 0.01;

	glBegin (GL_QUADS);
		// center rectangle
		glVertex3d (X + radius, Y - radius, 0);
		glVertex3d (X + width - radius, Y - radius, 0);
		glVertex3d (X + width - radius, Y - height + radius, 0);
		glVertex3d (X + radius, Y - height + radius, 0);
		// top rectangle
		glVertex3d ( X + radius, Y - radius, 0);
		glVertex3d ( X + width - radius, Y - radius, 0);
		glVertex3d ( X + width - radius, Y, 0);
		glVertex3d ( X + radius, Y, 0);
		// bottom rectangle
		glVertex3d ( X + radius, Y - height + radius, 0);
		glVertex3d ( X + width - radius, Y - height + radius, 0);
		glVertex3d ( X + width - radius, Y - height, 0);
		glVertex3d ( X + radius, Y - height, 0);
		// left rectangle
		glVertex3d ( X + radius, Y - radius, 0);
		glVertex3d ( X, Y - radius, 0);
		glVertex3d ( X, Y - height + radius, 0);
		glVertex3d ( X + radius, Y - height + radius, 0);
		// right rectangle
		glVertex3d ( X + width, Y - radius, 0);
		glVertex3d ( X + width - radius, Y - radius, 0);
		glVertex3d ( X + width - radius, Y - height + radius, 0);
		glVertex3d ( X + width, Y - height + radius, 0);
	glEnd();

	// corners
	glBegin (GL_POLYGON);
		// top left
		float xcoord = X + radius;
		float ycoord = Y - radius;
		glVertex3d (xcoord, ycoord, 0);
		for (float angle = M_PI/2; angle <= M_PI; angle += step) {
			glVertex3d ( radius * cos(angle) + xcoord, radius * sin(angle) + ycoord, 0);
		}
	glEnd();
	glBegin (GL_POLYGON);
		// top right
		xcoord = X + width - radius;
		ycoord = Y - radius;
		glVertex3d (xcoord, ycoord, 0);
		for (float angle = 0; angle <= M_PI/2; angle += step) {
			glVertex3d ( radius * cos(angle) + xcoord, radius * sin(angle) + ycoord, 0);
		}
	glEnd();
	glBegin (GL_POLYGON);
		// bottom left
		xcoord = X + radius;
		ycoord = Y - height + radius;
		glVertex3d (xcoord, ycoord, 0);
		for (float angle = -M_PI; angle <= -M_PI/2; angle += step) {
			glVertex3d ( radius * cos(angle) + xcoord, radius * sin(angle) + ycoord, 0);
		}
	glEnd();
	glBegin (GL_POLYGON);
		// bottom right
		xcoord = X + width - radius;
		ycoord = Y - height + radius;
		glVertex3d (xcoord, ycoord, 0);
		for (float angle = -M_PI/2; angle <= 0; angle += step) {
			glVertex3d ( radius * cos(angle) + xcoord, radius * sin(angle) + ycoord, 0);
		}
	glEnd();

	// block outline (and external block text)
	if (is_current_block(Block)) {
		glColor3f (1.0, 0.55, 0.0);
	}
	else {
		glColor3f (1.0, 1.0, 1.0);
	}
	glBegin (GL_LINES);
		// top rectangle contour
		glVertex3d ( X + radius, Y, 0);
		glVertex3d ( X + width - radius, Y, 0);
		// bottom rectangle contour
		glVertex3d ( X + radius, Y - height, 0);
		glVertex3d ( X + width - radius, Y - height, 0);
		// left rectangle contour
		glVertex3d ( X, Y - height + radius, 0);
		glVertex3d ( X, Y - radius, 0);
		// right rectangle contour
		glVertex3d ( X + width, Y - height + radius, 0);
		glVertex3d ( X + width, Y - radius, 0);
	glEnd();

	// corners
	glBegin (GL_LINE_STRIP);
		// top left
		xcoord = X + radius;
		ycoord = Y - radius;
		for (float angle = M_PI/2; angle <= M_PI; angle += step) {
			glVertex3d ( radius * cos(angle) + xcoord, radius * sin(angle) + ycoord, 0);
		}
	glEnd();
	glBegin (GL_LINE_STRIP);
		// top right
		xcoord = X + width - radius;
		ycoord = Y - radius;
		for (float angle = 0; angle <= M_PI/2; angle += step) {
			glVertex3d ( radius * cos(angle) + xcoord, radius * sin(angle) + ycoord, 0);
		}
	glEnd();
	glBegin (GL_LINE_STRIP);
		// bottom left
		xcoord = X + radius;
		ycoord = Y - height + radius;
		for (float angle = -M_PI; angle <= -M_PI/2; angle += step) {
			glVertex3d ( radius * cos(angle) + xcoord, radius * sin(angle) + ycoord, 0);
		}
	glEnd();
	glBegin (GL_LINE_STRIP);
		// bottom right
		xcoord = X + width - radius;
		ycoord = Y - height + radius;
		for (float angle = -M_PI/2; angle <= 0; angle += step) {
			glVertex3d ( radius * cos(angle) + xcoord, radius * sin(angle) + ycoord, 0);
		}
	glEnd();
}

void opengl_view::draw_rolled_block_body (const shader_block* Block, const double X, const double Y) {

	const double width = Block->m_width;
	const unsigned long max_properties = std::max (Block->input_count(), (unsigned long)Block->m_outputs.size()); // cast required by some unusual compilers (e.g. gcc version 4.1.3 20070929 (prerelease))

	// set minimal block height
	const double height1 = m_services->is_rolled (Block) ? width : (width * (1.0 /3.7) * static_cast<double> (max_properties));
	const double height = (height1 < m_min_block_height) ? m_min_block_height : height1;

	const double alpha = 0.5;

	// check whether the block's selected
	const bool is_selected = m_services->is_selected (Block);
	// block color
	if (is_selected)
		// selected blocks are "hover orange"
		glColor4f (1.0, 0.55, 0.0, alpha);
	else if (!Block->m_inputs.size())
		// inputs are green
		glColor4f (0.0, 1.0, 0.0, alpha);
	else
		// other ones are blue
		glColor4f (0.2, 0.3, 1.0, alpha); // for rolled up blocks

	glBegin (GL_QUADS);
		glVertex3d ((2*X + width) / 2.0, Y, 0);
		glVertex3d (X + width, (2*Y - height) / 2.0, 0);
		glVertex3d ((2*X + width) / 2.0, Y - height, 0);
		glVertex3d (X, (2*Y - height) / 2.0, 0);
	glEnd();

	glColor3f (1.0, 1.0, 1.0);
	glBegin (GL_LINES);
		glVertex3d ((2*X + width) / 2.0, Y, 0);
		glVertex3d (X + width, (2*Y - height) / 2.0, 0);
		glVertex3d (X + width, (2*Y - height) / 2.0, 0);
		glVertex3d ((2*X + width) / 2.0, Y - height, 0);
		glVertex3d ((2*X + width) / 2.0, Y - height, 0);
		glVertex3d (X, (2*Y - height) / 2.0, 0);
		glVertex3d (X, (2*Y - height) / 2.0, 0);
		glVertex3d ((2*X + width) / 2.0, Y, 0);

		glVertex3d (X, (2*Y - height) / 2.0, 0);
		glVertex3d (X + width, (2*Y - height) / 2.0, 0);
	glEnd();
}


void opengl_view::draw_block_name (const shader_block* Block, const double X, const double Y) {

	// show block name
	glsetfont (fltk::HELVETICA_BOLD, (m_font_size+0.5) * m_size * 0.75); // gets a bit more readable if bold
	fltk::gldrawtext (Block->name().c_str(), (float)X, (float) (Y + .05), (float)0);
}


void opengl_view::draw_block_properties (const shader_block* Block, const double X, const double Y, positions_t& PropertyPositions, const bool Selection) {

	const double width = Block->m_width;

	// draw properties
	const double property_size = 1.0/5.0;

	// input properties
	double start_x = X - property_size / 2;
	double start_y = Y - property_size / 2;

	for (shader_block::properties_t::const_iterator input = Block->m_inputs.begin(); input != Block->m_inputs.end(); ++input) {

		if (!input->m_multi_operator_parent_name.empty()) {

			// skip it
			continue;
		}

		if (Selection) {
			glLoadName (m_property_index);
			m_property_indices.insert (std::make_pair (m_property_index, std::make_pair (Block, input->m_name)));
			++m_property_index;
		}

		std::string type = Block->input_type (input->m_name);
		// FIXME: Name.c_str() is input name, not description
		// show property name
		glsetfont (fltk::HELVETICA, m_font_size * m_size * 0.75); // scale with zoom level
		glColor4f (0.65, 0.77, 0.97, 1.0); // #a5c5f7 light blue
		fltk::gldrawtext (input->m_name.c_str(), (float) (start_x + property_size * 1.25), (float) (start_y - 0.135), (float)0);

		if (std::make_pair (Block->name(), input->m_name) == m_active_property)
			type = "selected";

		if (input->m_multi_operator_parent_name.empty())
			draw_property (input->m_name, type, start_x, start_y, property_size, input->is_multi_operator());

		PropertyPositions.insert (std::make_pair (shrimp::io_t (Block->name(), input->m_name), position (start_x + property_size / 2, start_y - property_size / 2)));

		start_y -= property_size * (3.0/2.0);
	}

	// output properties
	start_x = X + width - property_size / 2;
	start_y = Y - property_size / 2;
	for (shader_block::properties_t::const_iterator output = Block->m_outputs.begin(); output != Block->m_outputs.end(); ++output) {

		if (Selection) {
			glLoadName (m_property_index);
			m_property_indices.insert (std::make_pair (m_property_index, std::make_pair (Block, output->m_name)));
			++m_property_index;
		}

		std::string type = Block->output_type (output->m_name);

		if (std::make_pair (Block->name(), output->m_name) == m_active_property)
			type = "selected";

		draw_property (output->m_name, type, start_x, start_y, property_size);
		PropertyPositions.insert (std::make_pair (shrimp::io_t (Block->name(), output->m_name), position (start_x + property_size / 2, start_y - property_size / 2)));

		start_y -= property_size * (3.0/2.0);
	}

}

void opengl_view::draw_property (const std::string& Name, const std::string& Type, const double X, const double Y, const double Size, const bool Multi) {

	const double third = Size / 3.0;
	const double small = Size / 6.0;

	if ("selected" == Type) {

		// orange hover color
		glColor3f (1.0, 0.55, 0.0 );
		glBegin (GL_QUADS);
			glVertex3d (X, Y, 0);
			glVertex3d (X + Size, Y, 0);
			glVertex3d (X + Size, Y - Size, 0);
			glVertex3d (X, Y - Size, 0);
		glEnd();

		// show property name
		glsetfont (fltk::HELVETICA, m_font_size * m_size * 0.75); // scale with zoom level
		glColor4f (1.0, 0.55, 0.0, 1.0); // orange hover color
		// FIXME: Name.c_str() is input name, not description
		fltk::gldrawtext (Name.c_str(), (float) (X + Size * 1.25), (float) (Y - 0.135), (float)0);
	}
	else if ("colour" == Type || "color" == Type) {

		// R G B
		glColor3f (0.8, 0.8, 0.8);
		glBegin (GL_QUADS);
			glVertex3d (X, Y, 0);
			glVertex3d (X + Size, Y, 0);
			glVertex3d (X + Size, Y - Size, 0);
			glVertex3d (X, Y - Size, 0);
		glEnd();

		glBegin (GL_QUADS);
			glColor3f (1.0, 0.0, 0.0);
			glVertex3d (X + small , Y - small, 0);
			glVertex3d (X + Size - small, Y - small, 0);
			glVertex3d (X + Size - small, Y - third * 1.2, 0);
			glVertex3d (X + small, Y - third * 1.2, 0);

			glColor3f (0.0, 1.0, 0.0);
			glVertex3d (X + small, Y - third * 1.2, 0);
			glVertex3d (X + Size - small, Y - third * 1.2, 0);
			glVertex3d (X + Size - small, Y - 1.8 * third, 0);
			glVertex3d (X + small, Y - 1.8 * third, 0);

			glColor3f (0.0, 0.0, 1.0);
			glVertex3d (X + small, Y - 1.8 * third, 0);
			glVertex3d (X + Size - small, Y - 1.8 * third, 0);
			glVertex3d (X + Size - small, Y - 3*third + small, 0);
			glVertex3d (X + small, Y - 3*third + small, 0);
		glEnd();
	}
	else if ("point" == Type) {

		// circle
		glColor3f (0.45, 0.83, 0.97);
		glBegin (GL_QUADS);
			glVertex3d (X, Y, 0);
			glVertex3d (X + Size, Y, 0);
			glVertex3d (X + Size, Y - Size, 0);
			glVertex3d (X, Y - Size, 0);
		glEnd();

		glColor3f (0.03, 0.14, 0.17);
		const int sections = 16;
		const double radius = Size / 4.0;
		// draw a filled circle
		glBegin (GL_TRIANGLE_FAN);
//			glVertex3d (X, Y, 0);
			for (int i = 0; i <= sections; ++i) {
				const double angle = static_cast<double> (i) * 2 * M_PI / static_cast<double> (sections);
				glVertex3d ((X+Size/2.0) + radius * cos (angle), (Y-Size/2.0) + radius * sin (angle), 0);
			}
		glEnd();
	}
	else if ("vector" == Type) {

		// arrow
		glColor3f (0.80, 0.91, 0.31);
		glBegin (GL_QUADS);
			glVertex3d (X, Y, 0);
			glVertex3d (X + Size, Y, 0);
			glVertex3d (X + Size, Y - Size, 0);
			glVertex3d (X, Y - Size, 0);
		glEnd();

		glColor3f (0.11, 0.12, 0.09);
		glBegin (GL_LINES);
			glVertex3d (X + small, Y - Size + small, 0);
			glVertex3d (X + Size - small, Y - small, 0);

			glVertex3d (X + Size - small, Y - small, 0);
			glVertex3d (X + Size - small - third, Y - small, 0);

			glVertex3d (X + Size - small, Y - small, 0);
			glVertex3d (X + Size - small, Y - small - third, 0);
		glEnd();
	}
	else if ("normal" == Type) {

		// arrow
		glColor3f (0.97, 0.74, 0.28);
		glBegin (GL_QUADS);
			glVertex3d (X, Y, 0);
			glVertex3d (X + Size, Y, 0);
			glVertex3d (X + Size, Y - Size, 0);
			glVertex3d (X, Y - Size, 0);
		glEnd();

		glColor3f (0.14, 0.11, 0.04);
		glBegin (GL_LINES);

			glVertex3d ( X + Size/2, Y - Size + small, 0);
			glVertex3d ( X + Size/2, Y - small, 0 );

			glVertex3d ( X + Size/2, Y - small, 0 );
			glVertex3d ( X + Size - small, Y - small - third, 0 );

			glVertex3d ( X + Size/2, Y - small, 0 );
			glVertex3d ( X + small, Y - small - third, 0 );

			glVertex3d ( X + small, Y - Size + small, 0 );
			glVertex3d ( X + Size - small, Y - Size + small, 0 );

		glEnd();
	}
	else if ("string" == Type) {

		// two horizontal lines
		glColor3f (0.6, 0.6, 0.6);
		glBegin (GL_QUADS);
			glVertex3d (X, Y, 0);
			glVertex3d (X + Size, Y, 0);
			glVertex3d (X + Size, Y - Size, 0);
			glVertex3d (X, Y - Size, 0);
		glEnd();

		glColor3f (0.0, 0.0, 0.0);
		glBegin (GL_LINES);
			glVertex3d (X + small, Y - small, 0);
			glVertex3d (X + Size - small, Y - small, 0);

			glVertex3d (X + small, Y - 2*small, 0);
			glVertex3d (X + Size - small, Y - 2*small, 0);
		glEnd();
	}
	else if ("matrix" == Type) {

		// two horizontal lines
		glColor3f (0.79, 0.66, 0.89);
		glBegin (GL_QUADS);
			glVertex3d (X, Y, 0);
			glVertex3d (X + Size, Y, 0);
			glVertex3d (X + Size, Y - Size, 0);
			glVertex3d (X, Y - Size, 0);
		glEnd();

		glColor3f (0.0, 0.0, 0.0);
		glBegin (GL_LINES);
			glVertex3d (X + small, Y - third, 0);
			glVertex3d (X + small, Y - 2*third, 0);

			glVertex3d (X + Size - small, Y - third, 0);
			glVertex3d (X + Size - small, Y - 2*third, 0);
		glEnd();
	}
	else if ("array" == Type) {

		// Grey square
		glColor3f (0.8, 0.8, 0.8);
		glBegin (GL_QUADS);
			glVertex3d (X, Y, 0);
			glVertex3d (X + Size, Y, 0);
			glVertex3d (X + Size, Y - Size, 0);
			glVertex3d (X, Y - Size, 0);
		glEnd();

		glBegin (GL_QUADS);
			glColor3f (0.00, 0.00, 0.00);
			glVertex3d (X + small, Y - small, 0);
			glVertex3d (X + Size/2 - small, Y - small, 0);
			glVertex3d (X + Size/2 - small, Y - Size/2 + small, 0);
			glVertex3d (X + small, Y - Size/2 + small, 0);

			glVertex3d (X + Size/2 + small, Y - small, 0);
			glVertex3d (X + Size - small, Y - small, 0);
			glVertex3d (X + Size - small, Y - Size/2 + small, 0);
			glVertex3d (X + Size/2 + small, Y - Size/2 + small, 0);

			glVertex3d (X + small, Y - Size/2 - small, 0);
			glVertex3d (X + Size/2 - small, Y - Size/2 - small, 0);
			glVertex3d (X + Size/2 - small, Y - Size + small, 0);
			glVertex3d (X + small, Y - Size + small, 0);

			glVertex3d (X + Size/2 + small, Y - Size/2 - small, 0);
			glVertex3d (X + Size - small, Y - Size/2 - small, 0);
			glVertex3d (X + Size - small, Y - Size + small, 0);
			glVertex3d (X + Size/2 + small, Y - Size + small, 0);
		glEnd();
	}
	else { // float
		// blank
		glColor3f (0.8, 0.8, 0.8);
		glBegin (GL_QUADS);
			glVertex3d (X, Y, 0);
			glVertex3d (X + Size, Y, 0);
			glVertex3d (X + Size, Y - Size, 0);
			glVertex3d (X, Y - Size, 0);
		glEnd();
	}

	if (Multi) {
		glColor3f (1.0, 1.0, 0.0);
		glBegin (GL_LINES);
			glVertex3d (X - small, Y + small, 0);
			glVertex3d (X + Size + small, Y + small, 0);

			glVertex3d (X + Size + small, Y + small, 0);
			glVertex3d (X + Size + small, Y - Size - small, 0);

			glVertex3d (X + Size + small, Y - Size - small, 0);
			glVertex3d (X - small, Y - Size - small, 0);

			glVertex3d (X - small, Y - Size - small, 0);
			glVertex3d (X - small, Y + small, 0);
		glEnd();
	}
}

void opengl_view::draw_groups() {

	const double alpha = 0.5;

	for (group_position_t::const_iterator p = m_group_positions.begin(); p != m_group_positions.end(); ++p)
	{
		const int group = p->first;
		const double x = p->second.position_x;
		const double y = p->second.position_y;


		draw_group_body (x, y,group);

		// check whether the group's selected
		// show group name
		if (group == m_current_group)
		{
			// selected group are "hover orange"
			glColor4f (1.0, 0.55, 0.0, alpha);
		}
		else
		{
			glColor3f (1, 1, 1);
		}

		glsetfont (fltk::HELVETICA_BOLD, (m_font_size+0.5) * m_size * 0.75);

		std::string name = m_services->get_group_name (group);
		// position with hexagon/group radius taken into account
		fltk::gldrawtext (name.c_str(), (float) (x-0.3f), (float) (y+0.4f), (float)0);
	}
}

void opengl_view::draw_group_body (int Group)
{
	group_position_t::const_iterator g = m_group_positions.find (Group);
	if (g == m_group_positions.end()) {
		log() << error << "Unknown group" << std::endl;
		return;
	}

	draw_group_body (g->second.position_x, g->second.position_y,Group);
}

void opengl_view::draw_group_body (const double X, const double Y, const int current_group)
{
	const int sections = 6;
	const double radius = 0.4;
	const double alpha = 1;

	// check whether the group's selected
	const bool is_selected = m_services->is_group_selected (current_group);
	// block color
	if (is_selected){
		// selected blocks are "hover orange"
		glColor4f (1.0, 0.55, 0.0, alpha);
	}
	else {
		// draw a filled hexagon, "hover blue"
		glColor3f (0.23, 0.37, 0.80);
	}
	glBegin (GL_TRIANGLE_FAN);
		for (int i = 0; i <= sections; ++i) {
			const double angle = static_cast<double> (i) * 2 * M_PI / static_cast<double> (sections);
			glVertex3d (X + radius * cos (angle), Y + radius * sin (angle), 0);
		}
	glEnd();

	// draw a white hexagon around
		if (current_group == m_active_group)
			// selected group are "hover orange"
			{glColor4f (1.0, 0.55, 0.0, alpha);}
		else {glColor3f (1, 1, 1);}

	glBegin (GL_LINES);
		double prev_x = X + radius * cos (0);
		double prev_y = Y + radius * sin (0);
		for (int i = 1; i <= sections; ++i) {
			const double angle = static_cast<double> (i) * 2 * M_PI / static_cast<double> (sections);
			double new_x = X + radius * cos (angle);
			double new_y = Y + radius * sin (angle);
			glVertex3d (prev_x, prev_y, 0);
			glVertex3d (new_x, new_y, 0);
			prev_x = new_x;
			prev_y = new_y;
		}
	glEnd();
}


void opengl_view::set_grid_state (const bool GridState) {

	m_grid = GridState;
}


void opengl_view::set_snap_to_grid_state (const bool SnapState) {

	m_snap_to_grid = SnapState;
}


void opengl_view::set_overview_state (const bool OverviewState) {

	m_overview = OverviewState;
}

void opengl_view::snap_position (double& X, double& Y) {

	//const double grid_size = 1.0;
	const double snap_size = 0.3;

	const double x_floor = std::floor (X);
	const double x_ceil = std::ceil (X);

	if ((X - x_floor) < (x_ceil - X)) {

		if (X - x_floor < snap_size)
			X = x_floor;
	} else {

		if (x_ceil - X < snap_size)
			X = x_ceil;
	}

	const double y_floor = std::floor (Y);
	const double y_ceil = std::ceil (Y);

	if ((Y - y_floor) < (y_ceil - Y)) {

		if (Y - y_floor < snap_size)
			Y = y_floor;
	} else {

		if (y_ceil - Y < snap_size)
			Y = y_ceil;
	}
}


void opengl_view::set_console (console* console_instance) {

	m_console = console_instance;
}


void opengl_view::draw_console (int window_width, int window_height) {

	if (m_console) {

		// set constant size
		glsetfont (fltk::HELVETICA, 10);

		// draw lines at the bottom of the screen
		fltk::drawtext (m_console->get_lines(3).c_str(), fltk::Rectangle (window_width, window_height), fltk::ALIGN_LEFT | fltk::ALIGN_BOTTOM | fltk::ALIGN_WRAP);
	}
}


void opengl_view::mouse_wheel_update(double move)
{
	log() << aspect << "opengl_view: mouse_wheel_update, move=" << move << std::endl;

	double change = - 0.3 * move;

	set_size (m_size + change);
}


void opengl_view::mouse_move()
{
	log() << aspect << "opengl_view: mouse_move" << std::endl;

	// mouse move
	m_active_property = select_property();
	m_current_mouse_x = fltk::event_x();
	m_current_mouse_y = fltk::event_y();

	m_active_block = select_object();
	m_active_group = select_group();

	//Mouse move over a block, set block as current and draw the block border as selected
	if (m_active_block.size())
	{
		shader_block* block = m_services->get_block (m_active_block);
		set_current_block(block);
	}
	//Mouse move over a group, set group as current and draw the group border as selected
	else if (m_active_group)
	{
		set_current_group(m_active_group);
	}
	//Mouse over nothing
	else {

		set_current_block(NULL);
		set_current_group(0);
		m_box_selection = false;
	}
}


void opengl_view::mouse_any_button_down()
{
	log() << aspect << "opengl_view: mouse_any_button_down" << std::endl;

	m_mouse_click_x = fltk::event_x();
	m_mouse_click_y = fltk::event_y();

	m_last_mouse_x = fltk::event_x();
	m_last_mouse_y = fltk::event_y();

	m_mouse_click = 0;

	m_active_block = select_object();
	m_active_group = select_group();
}


void opengl_view::mouse_left_button_down()
{
	log() << aspect << "opengl_view: mouse_left_button_down" << std::endl;

	const bool shift_key_down = fltk::event_state (fltk::SHIFT);
	const bool ctrl_key_down = fltk::event_state (fltk::CTRL);

	if (ctrl_key_down) {

		if (m_active_block.size()) {

			shader_block* block = m_services->get_block (m_active_block);

			// toggle block selection
			m_services->set_block_selection (block, !m_services->is_selected (block));
			if (m_services->is_selected (block)) {
				m_current_selection_block= block;
			}
		}
		if (m_active_group)
		{
			if (m_services)
			{
				m_services->set_group_selection(m_active_group, !m_services->is_group_selected(m_active_group));
				is_selected_group = m_active_group;
			}
		}

	}

	else if (shift_key_down)
	{
		if (m_active_block.size())
		{
			shader_block* block = m_services->get_block (m_active_block);

			// toggle block selection
			m_services->set_block_selection (block, 1);
			if (m_services->is_selected (block))
			{
				m_current_selection_block= block;
			}
		}
		if (m_active_group)
		{
			m_services->set_group_selection(m_active_group, 1);
			is_selected_group = m_active_group;
		}
	}

	else if (m_active_property.first.size() && m_connection_start.first == "") {

		// save connection start
		m_connection_start_x = m_mouse_click_x;
		m_connection_start_y = m_mouse_click_y;
	}
	//Save selected group number
	else if (m_active_group) {
		is_selected_group = m_active_group;
		set_current_group(m_active_group);
	}

	else {
		if (!m_active_block.size() && (!m_active_group))
		{
			m_services->clear_selection();
			is_selected_group = 0;
		}

		m_start_drag_x = fltk::event_x();
		m_start_drag_y = fltk::event_y();

	}
}


void opengl_view::mouse_right_button_down()
{
	log() << aspect << "opengl_view: mouse_right_button_down" << std::endl;

	if (m_active_property.first.size())
	{
		// mouse is over a block property, call the right-click popup menu
		m_shader_property_right_click_signal.emit (m_active_property);

		// clear other actions
		m_mouse_click = 0;
		m_active_block = "";
		m_active_group = 0;
		m_active_property = std::make_pair ("", "");
		m_connection_start = std::make_pair ("", "");
	}
	else if (m_active_block.size())
	{
		// mouse is over a block (but not over a property)
		m_shader_block_right_click_signal.emit (m_active_block);

	}
	else if (m_active_group)
	{
		// mouse is over a group
		m_block_group_right_click_signal.emit (m_active_group);
	}
	else
	{
		// mouse's over nothing...

		// clear current actions
		m_mouse_click = 0;
		m_active_block = "";
		m_active_group = 0;
		m_active_property = std::make_pair ("", "");
		m_connection_start = std::make_pair ("", "");

		// selection menu
		if (m_services->selection_size() > 1)
		{
			m_empty_right_click_signal.emit();
		}
	}
}


void opengl_view::mouse_any_button_drag()
{
	log() << aspect << "opengl_view: mouse_any_button_drag" << std::endl;

	m_current_mouse_x = fltk::event_x();
	m_current_mouse_y = fltk::event_y();
}


void opengl_view::mouse_left_button_drag(const int widget_width, const int widget_height)
{
	log() << aspect << "opengl_view: mouse_left_button_drag" << std::endl;

	const bool alt_key_down =  fltk::event_state (fltk::ALT);

	m_mouse_click++;

	const double mouse_move_x = static_cast<double>(m_current_mouse_x - m_last_mouse_x);
	const double mouse_move_y = static_cast<double>(m_current_mouse_y - m_last_mouse_y);

	const double move_x = mouse_move_x * (m_projection_right - m_projection_left) / static_cast<double> (widget_width);
	const double move_y = - mouse_move_y * (m_projection_top - m_projection_bottom) / static_cast<double> (widget_height);

	if (m_active_property.first.size()) {

		// do nothing

	}
	else if (m_active_block.size()) {

		// move selected block
		move_active_block (move_x / m_size, move_y / m_size);

	}
	else if (m_active_group) {
		set_current_group(m_active_group);
		move_active_block (move_x / m_size, move_y / m_size);
		//move_active_group (move_x / m_size, move_y / m_size);
	}

	else if (alt_key_down ){
		// move scene when ALT key press
		move_scene (move_x, move_y);
	}
	else {

		//Drawing of rectangle selection
		m_box_selection=true;

	}

	m_last_mouse_x = m_current_mouse_x;
	m_last_mouse_y = m_current_mouse_y;
}


void opengl_view::mouse_left_button_release()
{
	log() << aspect << "opengl_view: mouse_left_button_release" << std::endl;

	const bool shift_key_down = fltk::event_state (fltk::SHIFT);
	const bool ctrl_key_down = fltk::event_state (fltk::CTRL);

	if (m_mouse_click <= 1) {

		if (m_active_property.first != "") {

			if (m_connection_start.first == "") {

				m_connection_start = m_active_property;
				m_mouse_click_x = fltk::event_x();
				m_mouse_click_y = fltk::event_y();
			}
			else {
				if (m_connection_start.first != m_active_property.first) {

					// connect properties
					shader_block* input_block = m_services->get_block(m_connection_start.first);
					shader_block* output_block = m_services->get_block(m_active_property.first);
					if (input_block && output_block) {

						if (input_block->is_output (m_connection_start.second) && output_block->is_input (m_active_property.second))
							std::swap (m_connection_start, m_active_property);

						m_services->connect (m_connection_start, m_active_property);
					}
				}

				m_connection_start = std::make_pair ("", "");
			}

		}
		//select block
		else if ((m_active_block.size() || m_active_group) &&  !(shift_key_down || ctrl_key_down))
		{
			shader_block* block = m_services->get_block (m_active_block);
			m_services->clear_selection();
			if (m_active_block.size()){
				// toggle block selection
				m_services->set_block_selection (block, !m_services->is_selected (block));
				if (m_services->is_selected (block))
				{
					m_current_selection_block= block;
				}
			}
			else if (m_active_group)
			{
				m_services->set_group_selection(m_active_group, !m_services->is_group_selected(m_active_group));
			}
		}
	}

	// actually snap block when drag ends
	if (m_snap_to_grid)
	{
		shader_block* block = m_services->get_block (m_active_block);

		if (block) {
			double x = block->m_position_x;
			double y = block->m_position_y;
			snap_position (x, y);

			block->m_position_x = x;
			block->m_position_y = y;
		}
	}
}


void opengl_view::select_block()
{
	log() << aspect << "opengl_view: select_block" << std::endl;

	shader_block* block = m_services->get_block (m_active_block);
	m_services->set_block_selection (block, true);
}


void opengl_view::deselect_block()
{
	log() << aspect << "opengl_view: deselect_block" << std::endl;

	shader_block* block = m_services->get_block (m_active_block);
	m_services->set_block_selection (block, false);
}


void opengl_view::roll_block()
{
	shader_block* block = m_services->get_block (m_active_block);
	m_services->set_block_rolled_state (block, true);
}


void opengl_view::unroll_block()
{
	shader_block* block = m_services->get_block (m_active_block);
	m_services->set_block_rolled_state (block, false);
}


