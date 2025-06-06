/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRINITY_GRID_H
#define TRINITY_GRID_H

/*
  @class Grid
  Grid is a logical segment of the game world represented inside TrinIty.
  Grid is bind at compile time to a particular type of object which
  we call it the object of interested.  There are many types of loader,
  specially, dynamic loader, static loader, or on-demand loader.  There's
  a subtle difference between dynamic loader and on-demand loader but
  this is implementation specific to the loader class.  From the
  Grid's perspective, the loader meets its API requirement is suffice.
*/

#include "Define.h"
#include "Errors.h"
#include "TypeContainerVisitor.h"

template
<
class WORLD_OBJECT_CONTAINER,
class GRID_OBJECT_CONTAINER
>
class Grid
{
    public:

        /** destructor to clean up its resources. This includes unloading the
        grid if it has not been unload.
        */
        ~Grid() { }

        /** an object of interested enters the grid
         */
        template<class SPECIFIC_OBJECT>
        void AddWorldObject(SPECIFIC_OBJECT *obj)
        {
            i_objects.template Insert<SPECIFIC_OBJECT>(obj);
            ASSERT(obj->IsInGrid());
        }

        /** an object of interested exits the grid
         */
        //Actually an unlink is enough, no need to go through the container
        //template<class SPECIFIC_OBJECT>
        //void RemoveWorldObject(SPECIFIC_OBJECT *obj)
        //{
        //    ASSERT(obj->GetGridRef().isValid());
        //    i_objects.template Remove<SPECIFIC_OBJECT>(obj);
        //    ASSERT(!obj->GetGridRef().isValid());
        //}

        // Visit grid objects
        template<class T>
        void Visit(TypeContainerVisitor<T, GRID_OBJECT_CONTAINER> &visitor)
        {
            visitor.Visit(i_container);
        }

        // Visit world objects
        template<class T>
        void Visit(TypeContainerVisitor<T, WORLD_OBJECT_CONTAINER> &visitor)
        {
            visitor.Visit(i_objects);
        }

        /** Returns the number of object within the grid.
         */
        template<class T>
        std::size_t GetWorldObjectCountInGrid() const
        {
            return i_objects.template Size<T>();
        }

        /** Inserts a container type object into the grid.
         */
        template<class SPECIFIC_OBJECT>
        void AddGridObject(SPECIFIC_OBJECT *obj)
        {
            i_container.template Insert<SPECIFIC_OBJECT>(obj);
            ASSERT(obj->IsInGrid());
        }

        /** Removes a containter type object from the grid
         */
        //template<class SPECIFIC_OBJECT>
        //void RemoveGridObject(SPECIFIC_OBJECT *obj)
        //{
        //    ASSERT(obj->GetGridRef().isValid());
        //    i_container.template Remove<SPECIFIC_OBJECT>(obj);
        //    ASSERT(!obj->GetGridRef().isValid());
        //}

        /** Returns the number of container type object within the grid.
         */
        template<class T>
        std::size_t GetGridObjectCountInGrid() const
        {
            return i_container.template Size<T>();
        }

    private:
        GRID_OBJECT_CONTAINER i_container;
        WORLD_OBJECT_CONTAINER i_objects;
};

#endif
