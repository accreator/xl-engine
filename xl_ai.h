/*

  Copyright (C) 2005-2014 Chengtao Chen
  Copyright (C) 2014 Chengtao Chen, Kai Sun
  Copyright (C) 2015-2016 Chengtao Chen, Kai Sun, Tianyi Hao

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef XL_AI_H_INCLUDED
#define XL_AI_H_INCLUDED

uint8_t XlSearch(POSFORMAT pos);
void    XlDispose();
void    XlCancel();
int     XlCheckLastMove(POSFORMAT *p_pos);

#endif //XL_AI_H_INCLUDED
