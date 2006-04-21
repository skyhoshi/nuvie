/*
 *  Party.cpp
 *  Nuvie
 *
 *  Created by Eric Fry on Sun Mar 23 2003.
 *  Copyright (c) 2003. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */
#include "nuvieDefs.h"

#include "U6misc.h"
#include "NuvieIO.h"
#include "Game.h"
#include "Converse.h"
#include "TimedEvent.h"
#include "Configuration.h"
#include "ActorManager.h"
#include "SoundManager.h"
#include "Player.h"
#include "Map.h"
#include "U6UseCode.h"
#include "PartyPathFinder.h"
#include "Party.h"

Party::Party(Configuration *cfg)
{
 config = cfg;
 map = NULL;
 pathfinder = NULL;

 formation = PARTY_FORM_STANDARD;
 num_in_party = 0;
 prev_leader_x = prev_leader_y = 0;
}

Party::~Party()
{
}

bool Party::init(Game *g, ActorManager *am)
{
 std::string formation_string;

 game = g;
 actor_manager = am;
 map = g->get_game_map();
 if(!pathfinder)
   pathfinder = new PartyPathFinder(this);

 autowalk=false;
 in_vehicle = false;

 config->value("config/party_formation", formation_string, "");
 if(formation_string == "row")
   formation = PARTY_FORM_ROW;
 else if(formation_string == "column")
   formation = PARTY_FORM_COLUMN;
 else if(formation_string == "delta")
   formation = PARTY_FORM_DELTA;
 else
   formation = PARTY_FORM_STANDARD;

 return true;
}

bool Party::load(NuvieIO *objlist)
{
 uint8 actor_num;
 uint16 i;

 autowalk = false;
 in_vehicle = false;

 objlist->seek(0xff0);
 num_in_party = objlist->read1();


 objlist->seek(0xf00);
 for(i=0;i<num_in_party;i++)
  {
   objlist->readToBuf((unsigned char *)member[i].name,14); // read in Player name.
  }
 objlist->seek(0xfe0);
 for(i=0;i<num_in_party;i++)
  {
   actor_num = objlist->read1();
   member[i].actor = actor_manager->get_actor(actor_num);
   member[i].actor->set_in_party(true);
   member[i].inactive = false; // false unless actor is asleep, or paralyzed (is_immobile)
  }

 objlist->seek(0x1c12); // combat mode flag
 in_combat_mode = (bool)objlist->read1();

 MapCoord leader_loc = get_leader_location(); // previous leader location
 prev_leader_x = leader_loc.x;
 prev_leader_y = leader_loc.y;

 reform_party();

 autowalk=false;
 // this may not be the proper way to get in_vehicle at start, but we havn't
 // found the relevant data in objlist yet (maybe only vehicle worktype?)
 MapCoord vehicle_loc = actor_manager->get_actor(0)->get_location();
 if(is_at(vehicle_loc.x, vehicle_loc.y, vehicle_loc.z))
  {
   set_in_vehicle(true);
   hide();
  }

 update_music();

 return true;
}

bool Party::save(NuvieIO *objlist)
{
 uint16 i;

 objlist->seek(0xff0);
 objlist->write1(num_in_party);


 objlist->seek(0xf00);
 for(i=0;i<num_in_party;i++)
  {
   objlist->writeBuf((unsigned char *)member[i].name,14);
  }

 objlist->seek(0xfe0);
 for(i=0;i<num_in_party;i++)
  {
   objlist->write1(member[i].actor->get_actor_num());
  }

 objlist->seek(0x1c12); // combat mode flag
 objlist->write1((uint8)in_combat_mode);

 return true;
}

bool Party::add_actor(Actor *actor)
{
 Converse *converse = game->get_converse();

 if(num_in_party < 16)
   {
    actor->set_in_party(true);
    member[num_in_party].actor = actor;
    member[num_in_party].inactive = false;
    strncpy(member[num_in_party].name, converse->npc_name(actor->id_n), 14);
    member[num_in_party].name[13] = '\0'; // make sure name is terminated
    member[num_in_party].combat_position = 0;
//    member[num_in_party].leader_x = member[0].actor->get_location().x;
//    member[num_in_party].leader_y = member[0].actor->get_location().y;

    num_in_party++;
    reform_party();
    return true;
   }

 return false;
}


// remove actor from member array shuffle remaining actors down if required.
bool Party::remove_actor(Actor *actor)
{
 uint8 i;

 for(i=0;i< num_in_party;i++)
  {
   if(member[i].actor->id_n == actor->id_n)
     {
      member[i].actor->set_in_party(false);
      if(i != (num_in_party - 1))
        {
         for(;i+1<num_in_party;i++)
           member[i] = member[i+1];
        }
      num_in_party--;

      reform_party();
      return true;
     }
  }

 return false;
}

void Party::split_gold()
{
}

void Party::gather_gold()
{
}

uint8 Party::get_party_size()
{
 return num_in_party;
}

Actor *Party::get_actor(uint8 member_num)
{
 if(num_in_party <= member_num)
   return NULL;

 return member[member_num].actor;
}

char *Party::get_actor_name(uint8 member_num)
{
 if(num_in_party <= member_num)
   return NULL;

 return member[member_num].name;
}


/* Returns position of actor in party or -1.
 */
sint8 Party::get_member_num(Actor *actor)
{
    for(int i=0; i < num_in_party; i++)
    {
        if(member[i].actor->id_n == actor->id_n)
            return(i);
    }
    return(-1);
}

sint8 Party::get_member_num(uint8 a)
{
 return(get_member_num(actor_manager->get_actor(a)));
}

uint8 Party::get_actor_num(uint8 member_num)
{
 if(num_in_party <= member_num)
   return 0; // hmm how should we handle this error.

 return member[member_num].actor->id_n;
}

/* Rearrange member slot positions based on the number of members and the
 * selected formation. Used only when adding or removing actors.
 */
void Party::reform_party()
{
    uint32 n;
    sint32 x, y, max_x;
    bool even_row;
    sint8 leader = get_leader();
    if(leader < 0 || num_in_party == 1)
        return;
    member[leader].form_x = 0; member[leader].form_y = 0;
    switch(formation)
    {
        case PARTY_FORM_COLUMN: // line up behind Avatar
            x = 0; y = 1;
            for(n = leader+1; n < num_in_party; n++)
            {
                member[n].form_x = x;
                member[n].form_y = y++;
                if(y == 5)
                {
                    x += 1;
                    y = 0;
                }
            }
            break;
        case PARTY_FORM_ROW: // line up left of Avatar
            x = -1; y = 0;
            for(n = leader+1; n < num_in_party; n++)
            {
                member[n].form_x = x--;
                member[n].form_y = y;
                if(x == -5)
                {
                    y += 1;
                    x = 0;
                }
            }
            break;
        case PARTY_FORM_DELTA: // open triangle formation with Avatar at front
            x = -1; y = 1;
            for(n = leader+1; n < num_in_party; n++)
            {
                member[n].form_x = x;
                member[n].form_y = y;
                // alternate X once, then move down
                x = -x;
                if(x == 0 || x < 0)
                {
                    x -= 1;
                    ++y;
                }
                if(y == 5) // start at top of triangle
                {
                    y -= ((-x) - 1);
                    x = 0;
                }
            }
            break;
//        case PARTY_FORM_COMBAT: // positions determined by COMBAT mode
//            break;
        case PARTY_FORM_STANDARD: // U6
        default:
            // put first follower behind or behind and to the left of Avatar
            member[leader+1].form_x = (num_in_party >= 3) ? -1 : 0;
            member[leader+1].form_y = 1;
            x = y = 1;
            even_row = false;
            for(n = leader+2, max_x = 1; n < num_in_party; n++)
            {
                member[n].form_x = x;
                member[n].form_y = y;
                // alternate & move X left/right by 2
                x = (x == 0) ? x - 2 : (x < 0) ? -x : -x - 2;
                if(x > max_x || (x < 0 && -x > max_x)) // reached row max.
                {
                    ++y; even_row = !even_row; // next row
                    ++max_x; // increase row max.
                    x = even_row ? 0 : -1; // next guy starts at center or left by 2
                }
            }
    }
}

/* Returns number of person leading the party (the first active member), or -1
 * if the party has no leader and can't move. */
sint8 Party::get_leader()
{
    for(int m = 0; m < num_in_party; m++)
        if(!member[m].inactive)
            return m;
    return -1;
}

/* Get map location of a party member. */
MapCoord Party::get_location(uint8 m)
{
    return(member[m].actor->get_location());
}

/* Get map location of the first active party member. */
MapCoord Party::get_leader_location()
{
    sint8 m = get_leader();
    MapCoord loc;
    if(m >= 0)
        loc = member[m].actor->get_location();
    return(loc);
}

/* Returns absolute location where party member `m' SHOULD be (based on party
 * formation and leader location.
 */
MapCoord Party::get_formation_coords(uint8 m)
{
    MapCoord a = get_location(m); // my location
    MapCoord l = get_leader_location(); // leader location
    if(get_leader() < 0)
        return(a);
    uint8 ldir = member[get_leader()].actor->get_direction(); // leader direciton
    // intended location
    return(MapCoord((ldir == NUVIE_DIR_N) ? l.x + member[m].form_x : // X
                    (ldir == NUVIE_DIR_E) ? l.x - member[m].form_y :
                    (ldir == NUVIE_DIR_S) ? l.x - member[m].form_x :
                    (ldir == NUVIE_DIR_W) ? l.x + member[m].form_y : a.x,
                    (ldir == NUVIE_DIR_N) ? l.y + member[m].form_y : // Y
                    (ldir == NUVIE_DIR_E) ? l.y + member[m].form_x :
                    (ldir == NUVIE_DIR_S) ? l.y - member[m].form_y :
                    (ldir == NUVIE_DIR_W) ? l.y - member[m].form_x : a.y,
                    a.z // Z
                   ));
}


/* Update the actual locations of the party actors on the map, so that they are
 * in the proper formation. */
void Party::follow(sint8 rel_x, sint8 rel_y)
{
    bool try_again[get_party_max()]; // true if a member needs to try first pass again
    sint8 leader = get_leader();
    if(leader <= -1)
        return;
    // set previous leader location first, just in case the leader changed
    prev_leader_x = member[get_leader()].actor->x - rel_x;
    prev_leader_y = member[get_leader()].actor->y - rel_y;
    // PASS 1: Keep actors chained together.
    for(uint32 p = (leader+1); p < num_in_party; p++)
    {
        try_again[p] = false;
        if(!pathfinder->follow_passA(p))
            try_again[p] = true;
    }
    // PASS 2: Catch up to party.
    for(uint32 p = (leader+1); p < num_in_party; p++)
    {
        if(try_again[p])
            pathfinder->follow_passA(p);
        pathfinder->follow_passB(p);
        if(!pathfinder->is_contiguous(p))
        {
            printf("%s is looking for %s.\n", get_actor_name(p), get_actor_name(get_leader()));
            pathfinder->seek_leader(p); // enter/update seek mode
        }
        else if(member[p].actor->get_pathfinder())
            pathfinder->end_seek(p);
    }
}

// Returns true if anyone in the party has a matching object.
bool Party::has_obj(uint16 obj_n, uint8 quality, bool match_zero_qual)
{
 uint16 i;

 for(i=0;i<num_in_party;i++)
  {
   if(member[i].actor->inventory_get_object(obj_n, quality, 0, true, match_zero_qual) != NULL) // we got a match
     return true;
  }

 return false;
}

// Removes the first occurence of an object in the party.
bool Party::remove_obj(uint16 obj_n, uint8 quality)
{
 uint16 i;
 Obj *obj;
 
 for(i = 0; i < num_in_party; i++)
   {
    obj = member[i].actor->inventory_get_object(obj_n, quality);
    if(obj != NULL)
       {
        if(member[i].actor->inventory_remove_obj(obj))
          {
           delete_obj(obj);
           return true;
          }
       }
   }
    
 return false;
}

// Returns the actor id of the first person in the party to have a matching object.
uint16 Party::who_has_obj(uint16 obj_n, uint8 quality)
{
    uint16 i;
    for(i = 0; i < num_in_party; i++)
    {
        if(member[i].actor->inventory_get_object(obj_n, quality) != NULL)
            return(member[i].actor->get_actor_num());
    }
    return(0);
}


/* Is EVERYONE in the party at or near the coordinates?
 */
bool Party::is_at(uint16 x, uint16 y, uint8 z, uint32 threshold)
{
    for(uint32 p = 0; p < num_in_party; p++)
    {
        MapCoord loc(x, y, z);
        if(!member[p].actor->is_nearby(loc, threshold))
            return(false);
    }
    return(true);
}

bool Party::is_at(MapCoord &xyz, uint32 threshold)
{
 return(is_at(xyz.x,xyz.y,xyz.z,threshold));
}

/* Is ANYONE in the party at or near the coordinates? */
bool Party::is_anyone_at(uint16 x, uint16 y, uint8 z, uint32 threshold)
{
    for(uint32 p = 0; p < num_in_party; p++)
    {
        MapCoord loc(x, y, z);
        if(member[p].actor->is_nearby(loc, threshold))
            return(true);
    }
    return(false);
}

bool Party::is_anyone_at(MapCoord &xyz, uint32 threshold)
{
 return(is_anyone_at(xyz.x,xyz.y,xyz.z,threshold));
}

bool Party::contains_actor(Actor *actor)
{
 if(get_member_num(actor) >= 0)
    return(true);

 return(false);
}

bool Party::contains_actor(uint8 actor_num)
{
 return(contains_actor(actor_manager->get_actor(actor_num)));
}

void Party::set_in_combat_mode(bool value)
{
  // You can't enter combat mode while in a vehicle.
  if(value && in_vehicle)
     return;

  in_combat_mode = value;

  update_music();
}

void Party::update_music()
{
 SoundManager *s = Game::get_game()->get_sound_manager();
 MapCoord pos;

 if(in_vehicle)
   {
    s->musicPlayFrom("boat");
    return;
   }
 else if(in_combat_mode)
   {
    s->musicPlayFrom("combat");
    return;
   }

 pos = get_leader_location();

 switch(pos.z)
  {
   case 0 : s->musicPlayFrom("random"); break;
   case 5 : s->musicPlayFrom("gargoyle"); break;
   default : s->musicPlayFrom("dungeon"); break;
  }

 return;
}

void Party::heal()
{
 uint16 i;

 for(i=0;i<num_in_party;i++)
  {
   member[i].actor->heal();
  }

 return;
 
}

void Party::show()
{
 uint16 i;

 for(i=0;i<num_in_party;i++)
  {
   member[i].actor->show();
  }

 return;
}

void Party::hide()
{
 uint16 i;

 for(i=0;i<num_in_party;i++)
  {
   member[i].actor->hide();
  }

 return;
}

/* Move and center everyone in the party to one location.
 */
bool Party::move(uint16 dx, uint16 dy, uint8 dz)
{
    for(sint32 m = 0; m < num_in_party; m++)
        if(!member[m].actor->move(dx, dy, dz, ACTOR_FORCE_MOVE))
            return(false);
    return(true);
}


/* Automatically walk (timed) to a destination, and then teleport to new
 * location (optional). Used to enter/exit dungeons.
 * (step_delay 0 = default speed)
 */
void Party::walk(MapCoord *walkto, MapCoord *teleport, uint32 step_delay)
{
    if(step_delay)
        new TimedPartyMove(walkto, teleport, step_delay);
    else
        new TimedPartyMove(walkto, teleport);

    game->pause_world(); // other actors won't move
    game->pause_user(); // don't allow input
    // view will snap back to player after everyone has moved
    game->get_player()->set_mapwindow_centered(false);
    autowalk = true;
}


/* Enter a moongate and teleport to a new location.
 * (step_delay 0 = default speed)
 */
void Party::walk(Obj *moongate, MapCoord *teleport, uint32 step_delay)
{
    MapCoord walkto(moongate->x, moongate->y, moongate->z);
    if(step_delay)
        new TimedPartyMove(&walkto, teleport, moongate, step_delay);
    else
        new TimedPartyMove(&walkto, teleport, moongate);

    game->pause_world(); // other actors won't move
    game->pause_user(); // don't allow input
    // view will snap back to player after everyone has moved
    game->get_player()->set_mapwindow_centered(false);
    autowalk = true;
}



/* Automatically walk (timed) to vehicle. (step_delay 0 = default speed)
 */
void Party::enter_vehicle(Obj *ship_obj, uint32 step_delay)
{
    MapCoord walkto(ship_obj->x, ship_obj->y, ship_obj->z);

    dismount_from_horses();

    if(step_delay)
        new TimedPartyMoveToVehicle(&walkto, ship_obj, step_delay);
    else
        new TimedPartyMoveToVehicle(&walkto, ship_obj);

    game->pause_world(); // other actors won't move
    game->pause_user(); // don't allow input
    // view will snap back to player after everyone has moved
    game->get_player()->set_mapwindow_centered(false);
    autowalk = true;
}

void Party::set_in_vehicle(bool value)
{
 in_vehicle = value;

 update_music();

 return;
}

/* Done automatically walking, return view to player character.
 */
void Party::stop_walking()
{
    game->get_player()->set_mapwindow_centered(true);
    game->unpause_world(); // allow user input, unfreeze actors
    game->unpause_user();
    autowalk = false;
    update_music();
}

void Party::dismount_from_horses()
{
 UseCode *usecode = Game::get_game()->get_usecode();

 for(uint32 m = 0; m < num_in_party; m++)
   {
    if(member[m].actor->obj_n == OBJ_U6_HORSE_WITH_RIDER)
      {
       Obj *my_obj = member[m].actor->make_obj();
       usecode->use_obj(my_obj, member[m].actor);
       delete my_obj;
      }
   }

 return;
}
