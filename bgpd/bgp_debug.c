/* BGP-4, BGP-4+ packet debug routine
   Copyright (C) 1996, 97, 99 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include <zebra.h>

#include <lib/version.h>
#include "prefix.h"
#include "linklist.h"
#include "stream.h"
#include "command.h"
#include "str.h"
#include "log.h"
#include "sockunion.h"
#include "memory.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_aspath.h"
#include "bgpd/bgp_route.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_debug.h"
#include "bgpd/bgp_community.h"

unsigned long conf_bgp_debug_as4;
unsigned long conf_bgp_debug_neighbor_events;
unsigned long conf_bgp_debug_events;
unsigned long conf_bgp_debug_packet;
unsigned long conf_bgp_debug_filter;
unsigned long conf_bgp_debug_keepalive;
unsigned long conf_bgp_debug_update;
unsigned long conf_bgp_debug_zebra;
unsigned long conf_bgp_debug_nht;

unsigned long term_bgp_debug_as4;
unsigned long term_bgp_debug_neighbor_events;
unsigned long term_bgp_debug_events;
unsigned long term_bgp_debug_packet;
unsigned long term_bgp_debug_filter;
unsigned long term_bgp_debug_keepalive;
unsigned long term_bgp_debug_update;
unsigned long term_bgp_debug_zebra;
unsigned long term_bgp_debug_nht;

struct list *bgp_debug_neighbor_events_peers = NULL;
struct list *bgp_debug_keepalive_peers = NULL;
struct list *bgp_debug_update_out_peers = NULL;
struct list *bgp_debug_update_in_peers = NULL;
struct list *bgp_debug_update_prefixes = NULL;
struct list *bgp_debug_zebra_prefixes = NULL;

/* messages for BGP-4 status */
const struct message bgp_status_msg[] = 
{
  { Idle, "Idle" },
  { Connect, "Connect" },
  { Active, "Active" },
  { OpenSent, "OpenSent" },
  { OpenConfirm, "OpenConfirm" },
  { Established, "Established" },
  { Clearing,    "Clearing"    },
  { Deleted,     "Deleted"     },
};
const int bgp_status_msg_max = BGP_STATUS_MAX;

/* BGP message type string. */
const char *bgp_type_str[] =
{
  NULL,
  "OPEN",
  "UPDATE",
  "NOTIFICATION",
  "KEEPALIVE",
  "ROUTE-REFRESH",
  "CAPABILITY"
};

/* message for BGP-4 Notify */
static const struct message bgp_notify_msg[] = 
{
  { BGP_NOTIFY_HEADER_ERR, "Message Header Error"},
  { BGP_NOTIFY_OPEN_ERR, "OPEN Message Error"},
  { BGP_NOTIFY_UPDATE_ERR, "UPDATE Message Error"},
  { BGP_NOTIFY_HOLD_ERR, "Hold Timer Expired"},
  { BGP_NOTIFY_FSM_ERR, "Neighbor Events Error"},
  { BGP_NOTIFY_CEASE, "Cease"},
  { BGP_NOTIFY_CAPABILITY_ERR, "CAPABILITY Message Error"},
};
static const int bgp_notify_msg_max = BGP_NOTIFY_MAX;

static const struct message bgp_notify_head_msg[] = 
{
  { BGP_NOTIFY_HEADER_NOT_SYNC, "/Connection Not Synchronized"},
  { BGP_NOTIFY_HEADER_BAD_MESLEN, "/Bad Message Length"},
  { BGP_NOTIFY_HEADER_BAD_MESTYPE, "/Bad Message Type"}
};
static const int bgp_notify_head_msg_max = BGP_NOTIFY_HEADER_MAX;

static const struct message bgp_notify_open_msg[] = 
{
  { BGP_NOTIFY_SUBCODE_UNSPECIFIC, "/Unspecific"},
  { BGP_NOTIFY_OPEN_UNSUP_VERSION, "/Unsupported Version Number" },
  { BGP_NOTIFY_OPEN_BAD_PEER_AS, "/Bad Peer AS"},
  { BGP_NOTIFY_OPEN_BAD_BGP_IDENT, "/Bad BGP Identifier"},
  { BGP_NOTIFY_OPEN_UNSUP_PARAM, "/Unsupported Optional Parameter"},
  { BGP_NOTIFY_OPEN_AUTH_FAILURE, "/Authentication Failure"},
  { BGP_NOTIFY_OPEN_UNACEP_HOLDTIME, "/Unacceptable Hold Time"}, 
  { BGP_NOTIFY_OPEN_UNSUP_CAPBL, "/Unsupported Capability"},
};
static const int bgp_notify_open_msg_max = BGP_NOTIFY_OPEN_MAX;

static const struct message bgp_notify_update_msg[] = 
{
  { BGP_NOTIFY_SUBCODE_UNSPECIFIC, "/Unspecific"},
  { BGP_NOTIFY_UPDATE_MAL_ATTR, "/Malformed Attribute List"},
  { BGP_NOTIFY_UPDATE_UNREC_ATTR, "/Unrecognized Well-known Attribute"},
  { BGP_NOTIFY_UPDATE_MISS_ATTR, "/Missing Well-known Attribute"},
  { BGP_NOTIFY_UPDATE_ATTR_FLAG_ERR, "/Attribute Flags Error"},
  { BGP_NOTIFY_UPDATE_ATTR_LENG_ERR, "/Attribute Length Error"},
  { BGP_NOTIFY_UPDATE_INVAL_ORIGIN, "/Invalid ORIGIN Attribute"},
  { BGP_NOTIFY_UPDATE_AS_ROUTE_LOOP, "/AS Routing Loop"},
  { BGP_NOTIFY_UPDATE_INVAL_NEXT_HOP, "/Invalid NEXT_HOP Attribute"},
  { BGP_NOTIFY_UPDATE_OPT_ATTR_ERR, "/Optional Attribute Error"},
  { BGP_NOTIFY_UPDATE_INVAL_NETWORK, "/Invalid Network Field"},
  { BGP_NOTIFY_UPDATE_MAL_AS_PATH, "/Malformed AS_PATH"},
};
static const int bgp_notify_update_msg_max = BGP_NOTIFY_UPDATE_MAX;

static const struct message bgp_notify_cease_msg[] =
{
  { BGP_NOTIFY_SUBCODE_UNSPECIFIC, "/Unspecific"},
  { BGP_NOTIFY_CEASE_MAX_PREFIX, "/Maximum Number of Prefixes Reached"},
  { BGP_NOTIFY_CEASE_ADMIN_SHUTDOWN, "/Administratively Shutdown"},
  { BGP_NOTIFY_CEASE_PEER_UNCONFIG, "/Peer Unconfigured"},
  { BGP_NOTIFY_CEASE_ADMIN_RESET, "/Administratively Reset"},
  { BGP_NOTIFY_CEASE_CONNECT_REJECT, "/Connection Rejected"},
  { BGP_NOTIFY_CEASE_CONFIG_CHANGE, "/Other Configuration Change"},
  { BGP_NOTIFY_CEASE_COLLISION_RESOLUTION, "/Connection collision resolution"},
  { BGP_NOTIFY_CEASE_OUT_OF_RESOURCE, "/Out of Resource"},
};
static const int bgp_notify_cease_msg_max = BGP_NOTIFY_CEASE_MAX;

static const struct message bgp_notify_capability_msg[] = 
{
  { BGP_NOTIFY_SUBCODE_UNSPECIFIC, "/Unspecific"},
  { BGP_NOTIFY_CAPABILITY_INVALID_ACTION, "/Invalid Action Value" },
  { BGP_NOTIFY_CAPABILITY_INVALID_LENGTH, "/Invalid Capability Length"},
  { BGP_NOTIFY_CAPABILITY_MALFORMED_CODE, "/Malformed Capability Value"},
};
static const int bgp_notify_capability_msg_max = BGP_NOTIFY_CAPABILITY_MAX;

/* Origin strings. */
const char *bgp_origin_str[] = {"i","e","?"};
const char *bgp_origin_long_str[] = {"IGP","EGP","incomplete"};


/* Given a string return a pointer the corresponding peer structure */
static struct peer *
bgp_find_peer (struct vty *vty, const char *peer_str)
{
  int ret;
  union sockunion su;
  struct bgp *bgp;

  bgp = vty->index;
  ret = str2sockunion (peer_str, &su);

  /* 'swpX' string */
  if (ret < 0)
    return peer_lookup_by_conf_if (bgp, peer_str);
  else
    return peer_lookup (bgp, &su);
}

static void
bgp_debug_list_free(struct list *list)
{
  struct bgp_debug_filter *filter;
  struct listnode *node, *nnode;

  if (list)
    for (ALL_LIST_ELEMENTS (list, node, nnode, filter))
      {
        listnode_delete (list, filter);

        if (filter->p)
          prefix_free(filter->p);

        if (filter->peer)
          peer_unlock (filter->peer);

        XFREE (MTYPE_BGP_DEBUG_FILTER, filter);
      }
}

/* Print the desc along with a list of peers/prefixes this debug is
 * enabled for */
static void
bgp_debug_list_print (struct vty *vty, const char *desc, struct list *list)
{
  struct bgp_debug_filter *filter;
  struct listnode *node, *nnode;
  char buf[INET6_ADDRSTRLEN];

  vty_out (vty, "%s", desc);

  if (list && !list_isempty(list))
    {
      vty_out (vty, " for");
      for (ALL_LIST_ELEMENTS (list, node, nnode, filter))
        {
          if (filter->peer)
            vty_out (vty, " %s", filter->peer->host);

          if (filter->p)
            vty_out (vty, " %s/%d",
                     inet_ntop (filter->p->family, &filter->p->u.prefix, buf, INET6_ADDRSTRLEN),
                     filter->p->prefixlen);
        }
    }

  vty_out (vty, "%s", VTY_NEWLINE);
}

static void
bgp_debug_list_add_entry(struct list *list, struct peer *peer, struct prefix *p)
{
  struct bgp_debug_filter *filter;

  filter = XCALLOC (MTYPE_BGP_DEBUG_FILTER, sizeof (struct bgp_debug_filter));

  if (peer)
    {
      peer_lock (peer);
      filter->peer = peer;
      filter->p = NULL;
    }
  else if (p)
    {
      filter->peer = NULL;
      filter->p = p;
    }

  listnode_add(list, filter);
}

static int
bgp_debug_list_remove_entry(struct list *list, struct peer *peer, struct prefix *p)
{
  struct bgp_debug_filter *filter;
  struct listnode *node, *nnode;

  for (ALL_LIST_ELEMENTS (list, node, nnode, filter))
    {
      if (peer && filter->peer == peer)
        {
          listnode_delete (list, filter);
          peer_unlock (filter->peer);
          XFREE (MTYPE_BGP_DEBUG_FILTER, filter);
          return 1;
        }
      else if (p && filter->p->prefixlen == p->prefixlen && prefix_match(filter->p, p))
        {
          listnode_delete (list, filter);
          prefix_free (filter->p);
          XFREE (MTYPE_BGP_DEBUG_FILTER, filter);
          return 1;
        }
    }

  return 0;
}

static int
bgp_debug_list_has_entry(struct list *list, struct peer *peer, struct prefix *p)
{
  struct bgp_debug_filter *filter;
  struct listnode *node, *nnode;

  for (ALL_LIST_ELEMENTS (list, node, nnode, filter))
    {
      if (peer)
        {
          if (filter->peer == peer)
            {
              return 1;
            }
        }
      else if (p)
        {
          if (filter->p->prefixlen == p->prefixlen && prefix_match(filter->p, p))
            {
              return 1;
            }
        }
    }

  return 0;
}

/* Dump attribute. */
int
bgp_dump_attr (struct peer *peer, struct attr *attr, char *buf, size_t size)
{
  if (! attr)
    return 0;

  if (CHECK_FLAG (attr->flag, ATTR_FLAG_BIT (BGP_ATTR_NEXT_HOP)))
    snprintf (buf, size, "nexthop %s", inet_ntoa (attr->nexthop));

  if (CHECK_FLAG (attr->flag, ATTR_FLAG_BIT (BGP_ATTR_ORIGIN)))
    snprintf (buf + strlen (buf), size - strlen (buf), ", origin %s",
	      bgp_origin_str[attr->origin]);

#ifdef HAVE_IPV6
  if (attr->extra)
    {
      char addrbuf[BUFSIZ];

      /* Add MP case. */
      if (attr->extra->mp_nexthop_len == 16 
          || attr->extra->mp_nexthop_len == 32)
        snprintf (buf + strlen (buf), size - strlen (buf), ", mp_nexthop %s",
                  inet_ntop (AF_INET6, &attr->extra->mp_nexthop_global, 
                             addrbuf, BUFSIZ));

      if (attr->extra->mp_nexthop_len == 32)
        snprintf (buf + strlen (buf), size - strlen (buf), "(%s)",
                  inet_ntop (AF_INET6, &attr->extra->mp_nexthop_local, 
                             addrbuf, BUFSIZ));
    }
#endif /* HAVE_IPV6 */

  if (CHECK_FLAG (attr->flag, ATTR_FLAG_BIT (BGP_ATTR_LOCAL_PREF)))
    snprintf (buf + strlen (buf), size - strlen (buf), ", localpref %u",
	      attr->local_pref);

  if (CHECK_FLAG (attr->flag, ATTR_FLAG_BIT (BGP_ATTR_MULTI_EXIT_DISC))) 
    snprintf (buf + strlen (buf), size - strlen (buf), ", metric %u",
	      attr->med);

  if (CHECK_FLAG (attr->flag, ATTR_FLAG_BIT (BGP_ATTR_COMMUNITIES))) 
    snprintf (buf + strlen (buf), size - strlen (buf), ", community %s",
	      community_str (attr->community));

  if (CHECK_FLAG (attr->flag, ATTR_FLAG_BIT (BGP_ATTR_ATOMIC_AGGREGATE)))
    snprintf (buf + strlen (buf), size - strlen (buf), ", atomic-aggregate");

  if (CHECK_FLAG (attr->flag, ATTR_FLAG_BIT (BGP_ATTR_AGGREGATOR)))
    snprintf (buf + strlen (buf), size - strlen (buf), ", aggregated by %u %s",
	      attr->extra->aggregator_as,
	      inet_ntoa (attr->extra->aggregator_addr));

  if (CHECK_FLAG (attr->flag, ATTR_FLAG_BIT (BGP_ATTR_ORIGINATOR_ID)))
    snprintf (buf + strlen (buf), size - strlen (buf), ", originator %s",
	      inet_ntoa (attr->extra->originator_id));

  if (CHECK_FLAG (attr->flag, ATTR_FLAG_BIT (BGP_ATTR_CLUSTER_LIST)))
    {
      int i;

      snprintf (buf + strlen (buf), size - strlen (buf), ", clusterlist");
      for (i = 0; i < attr->extra->cluster->length / 4; i++)
	snprintf (buf + strlen (buf), size - strlen (buf), " %s",
		  inet_ntoa (attr->extra->cluster->list[i]));
    }

  if (CHECK_FLAG (attr->flag, ATTR_FLAG_BIT (BGP_ATTR_AS_PATH))) 
    snprintf (buf + strlen (buf), size - strlen (buf), ", path %s",
	      aspath_print (attr->aspath));

  if (strlen (buf) > 1)
    return 1;
  else
    return 0;
}

/* dump notify packet */
void
bgp_notify_print(struct peer *peer, struct bgp_notify *bgp_notify, 
                 const char *direct)
{
  const char *subcode_str;
  const char *code_str;

  subcode_str = "";
  code_str = LOOKUP_DEF (bgp_notify_msg, bgp_notify->code,
                         "Unrecognized Error Code");

  switch (bgp_notify->code)
    {
    case BGP_NOTIFY_HEADER_ERR:
      subcode_str = LOOKUP_DEF (bgp_notify_head_msg, bgp_notify->subcode,
                                "Unrecognized Error Subcode");
      break;
    case BGP_NOTIFY_OPEN_ERR:
      subcode_str = LOOKUP_DEF (bgp_notify_open_msg, bgp_notify->subcode,
                                "Unrecognized Error Subcode");
      break;
    case BGP_NOTIFY_UPDATE_ERR:
      subcode_str = LOOKUP_DEF (bgp_notify_update_msg, bgp_notify->subcode,
                                "Unrecognized Error Subcode");
      break;
    case BGP_NOTIFY_HOLD_ERR:
      break;
    case BGP_NOTIFY_FSM_ERR:
      break;
    case BGP_NOTIFY_CEASE:
      subcode_str = LOOKUP_DEF (bgp_notify_cease_msg, bgp_notify->subcode,
                                "Unrecognized Error Subcode");
      break;
    case BGP_NOTIFY_CAPABILITY_ERR:
      subcode_str = LOOKUP_DEF (bgp_notify_capability_msg, bgp_notify->subcode,
                                "Unrecognized Error Subcode");
      break;
    }

  if (BGP_DEBUG (neighbor_events, NEIGHBOR_EVENTS) ||
      bgp_flag_check (peer->bgp, BGP_FLAG_LOG_NEIGHBOR_CHANGES))
    zlog_info ("%%NOTIFICATION: %s neighbor %s %d/%d (%s%s) %d bytes %s",
              strcmp (direct, "received") == 0 ? "received from" : "sent to",
              peer->host, bgp_notify->code, bgp_notify->subcode,
              code_str, subcode_str, bgp_notify->length,
              bgp_notify->data ? bgp_notify->data : "");
}

/* Debug option setting interface. */
unsigned long bgp_debug_option = 0;

int  
debug (unsigned int option)
{
  return bgp_debug_option & option; 
}

DEFUN (debug_bgp_as4,
       debug_bgp_as4_cmd,
       "debug bgp as4",
       DEBUG_STR
       BGP_STR
       "BGP AS4 actions\n")
{
  if (vty->node == CONFIG_NODE)
    DEBUG_ON (as4, AS4);
  else
    {
      TERM_DEBUG_ON (as4, AS4);
      vty_out (vty, "BGP as4 debugging is on%s", VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

DEFUN (no_debug_bgp_as4,
       no_debug_bgp_as4_cmd,
       "no debug bgp as4",
       NO_STR
       DEBUG_STR
       BGP_STR
       "BGP AS4 actions\n")
{
  if (vty->node == CONFIG_NODE)
    DEBUG_OFF (as4, AS4);
  else
    {
      TERM_DEBUG_OFF (as4, AS4);
      vty_out (vty, "BGP as4 debugging is off%s", VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

DEFUN (debug_bgp_as4_segment,
       debug_bgp_as4_segment_cmd,
       "debug bgp as4 segment",
       DEBUG_STR
       BGP_STR
       "BGP AS4 actions\n"
       "BGP AS4 aspath segment handling\n")
{
  if (vty->node == CONFIG_NODE)
    DEBUG_ON (as4, AS4_SEGMENT);
  else
    {
      TERM_DEBUG_ON (as4, AS4_SEGMENT);
      vty_out (vty, "BGP as4 segment debugging is on%s", VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

DEFUN (no_debug_bgp_as4_segment,
       no_debug_bgp_as4_segment_cmd,
       "no debug bgp as4 segment",
       NO_STR
       DEBUG_STR
       BGP_STR
       "BGP AS4 actions\n"
       "BGP AS4 aspath segment handling\n")
{
  if (vty->node == CONFIG_NODE)
    DEBUG_OFF (as4, AS4_SEGMENT);
  else
    {
      TERM_DEBUG_OFF (as4, AS4_SEGMENT);
      vty_out (vty, "BGP as4 segment debugging is off%s", VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

/* debug bgp neighbor_events */
DEFUN (debug_bgp_neighbor_events,
       debug_bgp_neighbor_events_cmd,
       "debug bgp neighbor-events",
       DEBUG_STR
       BGP_STR
       "BGP Neighbor Events\n")
{
  if (vty->node == CONFIG_NODE)
    DEBUG_ON (neighbor_events, NEIGHBOR_EVENTS);
  else
    {
      TERM_DEBUG_ON (neighbor_events, NEIGHBOR_EVENTS);
      vty_out (vty, "BGP neighbor-events debugging is on%s", VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

DEFUN (debug_bgp_neighbor_events_peer,
       debug_bgp_neighbor_events_peer_cmd,
       "debug bgp neighbor-events (A.B.C.D|X:X::X:X|WORD)",
       DEBUG_STR
       BGP_STR
       "BGP Neighbor Events\n"
       "BGP neighbor IP address to debug\n"
       "BGP IPv6 neighbor to debug\n"
       "BGP neighbor on interface to debug\n")
{
  struct peer *peer;

  peer = bgp_find_peer (vty, argv[0]);
  if (!peer)
    {
      vty_out (vty, "%s is not a configured peer%s", argv[0], VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (!bgp_debug_neighbor_events_peers)
    bgp_debug_neighbor_events_peers = list_new ();

  if (bgp_debug_list_has_entry(bgp_debug_neighbor_events_peers, peer, NULL))
    {
      vty_out (vty, "BGP neighbor-events debugging is already enabled for %s%s", peer->host, VTY_NEWLINE);
      return CMD_SUCCESS;
    }

  bgp_debug_list_add_entry(bgp_debug_neighbor_events_peers, peer, NULL);

  if (vty->node == CONFIG_NODE)
    DEBUG_ON (neighbor_events, NEIGHBOR_EVENTS);
  else
    {
      TERM_DEBUG_ON (neighbor_events, NEIGHBOR_EVENTS);
      vty_out (vty, "BGP neighbor-events debugging is on for %s%s", argv[0], VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

DEFUN (no_debug_bgp_neighbor_events,
       no_debug_bgp_neighbor_events_cmd,
       "no debug bgp neighbor-events",
       NO_STR
       DEBUG_STR
       BGP_STR
       "Neighbor Events\n")
{
  bgp_debug_list_free(bgp_debug_neighbor_events_peers);

  if (vty->node == CONFIG_NODE)
    DEBUG_OFF (neighbor_events, NEIGHBOR_EVENTS);
  else
    {
      TERM_DEBUG_OFF (neighbor_events, NEIGHBOR_EVENTS);
      vty_out (vty, "BGP neighbor-events debugging is off%s", VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

DEFUN (no_debug_bgp_neighbor_events_peer,
       no_debug_bgp_neighbor_events_peer_cmd,
       "no debug bgp neighbor-events (A.B.C.D|X:X::X:X|WORD)",
       NO_STR
       DEBUG_STR
       BGP_STR
       "Neighbor Events\n"
       "BGP neighbor IP address to debug\n"
       "BGP IPv6 neighbor to debug\n"
       "BGP neighbor on interface to debug\n")
{
  int found_peer = 0;
  struct peer *peer;

  peer = bgp_find_peer (vty, argv[0]);
  if (!peer)
    {
      vty_out (vty, "%s is not a configured peer%s", argv[0], VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (bgp_debug_neighbor_events_peers && !list_isempty(bgp_debug_neighbor_events_peers))
    {
      found_peer = bgp_debug_list_remove_entry(bgp_debug_neighbor_events_peers, peer, NULL);

      if (list_isempty(bgp_debug_neighbor_events_peers))
        {
          if (vty->node == CONFIG_NODE)
            DEBUG_OFF (neighbor_events, NEIGHBOR_EVENTS);
          else
            TERM_DEBUG_OFF (neighbor_events, NEIGHBOR_EVENTS);
        }
    }

  if (found_peer)
    vty_out (vty, "BGP neighbor-events debugging is off for %s%s", argv[0], VTY_NEWLINE);
  else
    vty_out (vty, "BGP neighbor-events debugging was not enabled for %s%s", argv[0], VTY_NEWLINE);

  return CMD_SUCCESS;
}

/* debug bgp nht */
DEFUN (debug_bgp_nht,
       debug_bgp_nht_cmd,
       "debug bgp nht",
       DEBUG_STR
       BGP_STR
       "BGP nexthop tracking events\n")
{
  if (vty->node == CONFIG_NODE)
    DEBUG_ON (nht, NHT);
  else
    {
      TERM_DEBUG_ON (nht, NHT);
      vty_out (vty, "BGP nexthop tracking debugging is on%s", VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

DEFUN (no_debug_bgp_nht,
       no_debug_bgp_nht_cmd,
       "no debug bgp nht",
       NO_STR
       DEBUG_STR
       BGP_STR
       "BGP nexthop tracking events\n")
{
  if (vty->node == CONFIG_NODE)
    DEBUG_OFF (nht, NHT);
  else
    {
      TERM_DEBUG_OFF (nht, NHT);
      vty_out (vty, "BGP nexthop tracking debugging is off%s", VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

/* debug bgp keepalives */
DEFUN (debug_bgp_keepalive,
       debug_bgp_keepalive_cmd,
       "debug bgp keepalives",
       DEBUG_STR
       BGP_STR
       "BGP keepalives\n")
{
  if (vty->node == CONFIG_NODE)
    DEBUG_ON (keepalive, KEEPALIVE);
  else
    {
      TERM_DEBUG_ON (keepalive, KEEPALIVE);
      vty_out (vty, "BGP keepalives debugging is on%s", VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

DEFUN (debug_bgp_keepalive_peer,
       debug_bgp_keepalive_peer_cmd,
       "debug bgp keepalives (A.B.C.D|X:X::X:X|WORD)",
       DEBUG_STR
       BGP_STR
       "BGP Neighbor Events\n"
       "BGP neighbor IP address to debug\n"
       "BGP IPv6 neighbor to debug\n"
       "BGP neighbor on interface to debug\n")
{
  struct peer *peer;

  peer = bgp_find_peer (vty, argv[0]);
  if (!peer)
    {
      vty_out (vty, "%s is not a configured peer%s", argv[0], VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (!bgp_debug_keepalive_peers)
    bgp_debug_keepalive_peers = list_new ();

  if (bgp_debug_list_has_entry(bgp_debug_keepalive_peers, peer, NULL))
    {
      vty_out (vty, "BGP keepalive debugging is already enabled for %s%s", peer->host, VTY_NEWLINE);
      return CMD_SUCCESS;
    }

  bgp_debug_list_add_entry(bgp_debug_keepalive_peers, peer, NULL);

  if (vty->node == CONFIG_NODE)
    DEBUG_ON (keepalive, KEEPALIVE);
  else
    {
      TERM_DEBUG_ON (keepalive, KEEPALIVE);
      vty_out (vty, "BGP keepalives debugging is on for %s%s", argv[0], VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

DEFUN (no_debug_bgp_keepalive,
       no_debug_bgp_keepalive_cmd,
       "no debug bgp keepalives",
       NO_STR
       DEBUG_STR
       BGP_STR
       "BGP keepalives\n")
{
  bgp_debug_list_free(bgp_debug_keepalive_peers);

  if (vty->node == CONFIG_NODE)
    DEBUG_OFF (keepalive, KEEPALIVE);
  else
    {
      TERM_DEBUG_OFF (keepalive, KEEPALIVE);
      vty_out (vty, "BGP keepalives debugging is off%s", VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

DEFUN (no_debug_bgp_keepalive_peer,
       no_debug_bgp_keepalive_peer_cmd,
       "no debug bgp keepalives (A.B.C.D|X:X::X:X|WORD)",
       NO_STR
       DEBUG_STR
       BGP_STR
       "BGP keepalives\n"
       "BGP neighbor IP address to debug\n"
       "BGP IPv6 neighbor to debug\n"
       "BGP neighbor on interface to debug\n")
{
  int found_peer = 0;
  struct peer *peer;

  peer = bgp_find_peer (vty, argv[0]);
  if (!peer)
    {
      vty_out (vty, "%s is not a configured peer%s", argv[0], VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (bgp_debug_keepalive_peers && !list_isempty(bgp_debug_keepalive_peers))
    {
      found_peer = bgp_debug_list_remove_entry(bgp_debug_keepalive_peers, peer, NULL);

      if (list_isempty(bgp_debug_keepalive_peers))
        {
          if (vty->node == CONFIG_NODE)
            DEBUG_OFF (keepalive, KEEPALIVE);
          else
            TERM_DEBUG_OFF (keepalive, KEEPALIVE);
        }
    }

  if (found_peer)
    vty_out (vty, "BGP keepalives debugging is off for %s%s", argv[0], VTY_NEWLINE);
  else
    vty_out (vty, "BGP keepalives debugging was not enabled for %s%s", argv[0], VTY_NEWLINE);

  return CMD_SUCCESS;
}

/* debug bgp updates */
DEFUN (debug_bgp_update,
       debug_bgp_update_cmd,
       "debug bgp updates",
       DEBUG_STR
       BGP_STR
       "BGP updates\n")
{
  if (vty->node == CONFIG_NODE)
    {
      DEBUG_ON (update, UPDATE_IN);
      DEBUG_ON (update, UPDATE_OUT);
    }
  else
    {
      TERM_DEBUG_ON (update, UPDATE_IN);
      TERM_DEBUG_ON (update, UPDATE_OUT);
      vty_out (vty, "BGP updates debugging is on%s", VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

DEFUN (debug_bgp_update_direct,
       debug_bgp_update_direct_cmd,
       "debug bgp updates (in|out)",
       DEBUG_STR
       BGP_STR
       "BGP updates\n"
       "Inbound updates\n"
       "Outbound updates\n")
{
  if (vty->node == CONFIG_NODE)
    {
      if (strncmp ("i", argv[0], 1) == 0)
	{
	  DEBUG_OFF (update, UPDATE_OUT);
	  DEBUG_ON (update, UPDATE_IN);
	}
      else
	{	
	  DEBUG_OFF (update, UPDATE_IN);
	  DEBUG_ON (update, UPDATE_OUT);
	}
    }
  else
    {
      if (strncmp ("i", argv[0], 1) == 0)
	{
	  TERM_DEBUG_OFF (update, UPDATE_OUT);
	  TERM_DEBUG_ON (update, UPDATE_IN);
	  vty_out (vty, "BGP updates debugging is on (inbound)%s", VTY_NEWLINE);
	}
      else
	{
	  TERM_DEBUG_OFF (update, UPDATE_IN);
	  TERM_DEBUG_ON (update, UPDATE_OUT);
	  vty_out (vty, "BGP updates debugging is on (outbound)%s", VTY_NEWLINE);
	}
    }
  return CMD_SUCCESS;
}

DEFUN (debug_bgp_update_direct_peer,
       debug_bgp_update_direct_peer_cmd,
       "debug bgp updates (in|out) (A.B.C.D|X:X::X:X|WORD)",
       DEBUG_STR
       BGP_STR
       "BGP updates\n"
       "Inbound updates\n"
       "Outbound updates\n"
       "BGP neighbor IP address to debug\n"
       "BGP IPv6 neighbor to debug\n"
       "BGP neighbor on interface to debug\n")
{
  struct peer *peer;
  int inbound;

  peer = bgp_find_peer (vty, argv[1]);
  if (!peer)
    {
      vty_out (vty, "%s is not a configured peer%s", argv[1], VTY_NEWLINE);
      return CMD_WARNING;
    }


  if (!bgp_debug_update_in_peers)
    bgp_debug_update_in_peers = list_new ();

  if (!bgp_debug_update_out_peers)
    bgp_debug_update_out_peers = list_new ();

  if (strncmp ("i", argv[0], 1) == 0)
    inbound = 1;
  else
    inbound = 0;

  if (inbound)
    {
      if (bgp_debug_list_has_entry(bgp_debug_update_in_peers, peer, NULL))
        {
          vty_out (vty, "BGP inbound update debugging is already enabled for %s%s", peer->host, VTY_NEWLINE);
          return CMD_SUCCESS;
        }
    }

  else
    {
      if (bgp_debug_list_has_entry(bgp_debug_update_out_peers, peer, NULL))
        {
          vty_out (vty, "BGP outbound update debugging is already enabled for %s%s", peer->host, VTY_NEWLINE);
          return CMD_SUCCESS;
        }
    }

  if (inbound)
    bgp_debug_list_add_entry(bgp_debug_update_in_peers, peer, NULL);
  else
    bgp_debug_list_add_entry(bgp_debug_update_out_peers, peer, NULL);

  if (vty->node == CONFIG_NODE)
    {
      if (inbound)
	{
	  DEBUG_OFF (update, UPDATE_OUT);
	  DEBUG_ON (update, UPDATE_IN);
	}
      else
	{
	  DEBUG_OFF (update, UPDATE_IN);
	  DEBUG_ON (update, UPDATE_OUT);
	}
    }
  else
    {
      if (inbound)
	{
	  TERM_DEBUG_OFF (update, UPDATE_OUT);
	  TERM_DEBUG_ON (update, UPDATE_IN);
	  vty_out (vty, "BGP updates debugging is on (inbound) for %s%s", argv[1], VTY_NEWLINE);
	}
      else
	{
	  TERM_DEBUG_OFF (update, UPDATE_IN);
	  TERM_DEBUG_ON (update, UPDATE_OUT);
	  vty_out (vty, "BGP updates debugging is on (outbound) for %s%s", argv[1], VTY_NEWLINE);
	}
    }
  return CMD_SUCCESS;
}

DEFUN (no_debug_bgp_update_direct_peer,
       no_debug_bgp_update_direct_peer_cmd,
       "no debug bgp updates (in|out) (A.B.C.D|X:X::X:X|WORD)",
       NO_STR
       DEBUG_STR
       BGP_STR
       "BGP updates\n"
       "Inbound updates\n"
       "Outbound updates\n"
       "BGP neighbor IP address to debug\n"
       "BGP IPv6 neighbor to debug\n"
       "BGP neighbor on interface to debug\n")
{
  int inbound;
  int found_peer = 0;
  struct peer *peer;

  peer = bgp_find_peer (vty, argv[1]);
  if (!peer)
    {
      vty_out (vty, "%s is not a configured peer%s", argv[1], VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (strncmp ("i", argv[0], 1) == 0)
    inbound = 1;
  else
    inbound = 0;

  if (inbound && bgp_debug_update_in_peers &&
      !list_isempty(bgp_debug_update_in_peers))
    {
      found_peer = bgp_debug_list_remove_entry(bgp_debug_update_in_peers, peer, NULL);

      if (list_isempty(bgp_debug_update_in_peers))
        {
          if (vty->node == CONFIG_NODE)
            DEBUG_OFF (update, UPDATE_IN);
          else
            {
              TERM_DEBUG_OFF (update, UPDATE_IN);
              vty_out (vty, "BGP updates debugging (inbound) is off%s", VTY_NEWLINE);
            }
        }
    }

  if (!inbound && bgp_debug_update_out_peers &&
      !list_isempty(bgp_debug_update_out_peers))
    {
      found_peer = bgp_debug_list_remove_entry(bgp_debug_update_out_peers, peer, NULL);

      if (list_isempty(bgp_debug_update_out_peers))
        {
          if (vty->node == CONFIG_NODE)
            DEBUG_OFF (update, UPDATE_OUT);
          else
            {
              TERM_DEBUG_OFF (update, UPDATE_OUT);
              vty_out (vty, "BGP updates debugging (outbound) is off%s", VTY_NEWLINE);
            }
        }
    }

  if (found_peer)
    if (inbound)
      vty_out (vty, "BGP updates debugging (inbound) is off for %s%s", argv[1], VTY_NEWLINE);
    else
      vty_out (vty, "BGP updates debugging (outbound) is off for %s%s", argv[1], VTY_NEWLINE);
  else
    if (inbound)
      vty_out (vty, "BGP updates debugging (inbound) was not enabled for %s%s", argv[1], VTY_NEWLINE);
    else
      vty_out (vty, "BGP updates debugging (outbound) was not enabled for %s%s", argv[1], VTY_NEWLINE);

  return CMD_SUCCESS;
}

DEFUN (debug_bgp_update_prefix,
       debug_bgp_update_prefix_cmd,
       "debug bgp updates prefix (A.B.C.D/M|X:X::X:X/M)",
       DEBUG_STR
       BGP_STR
       "BGP updates\n"
       "Specify a prefix to debug\n"
       "IP prefix <network>/<length>, e.g., 35.0.0.0/8\n"
       "IPv6 prefix <network>/<length>\n")

{
  struct prefix *argv_p;
  int ret;

  argv_p = prefix_new();
  ret = str2prefix (argv[0], argv_p);
  if (!ret)
    {
      prefix_free(argv_p);
      vty_out (vty, "%% Malformed Prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }


  if (!bgp_debug_update_prefixes)
    bgp_debug_update_prefixes = list_new ();

  if (bgp_debug_list_has_entry(bgp_debug_update_prefixes, NULL, argv_p))
    {
      vty_out (vty, "BGP updates debugging is already enabled for %s%s", argv[0], VTY_NEWLINE);
      return CMD_SUCCESS;
    }

  bgp_debug_list_add_entry(bgp_debug_update_prefixes, NULL, argv_p);

  if (vty->node == CONFIG_NODE)
    {
      DEBUG_ON (update, UPDATE_PREFIX);
    }
  else
    {
      TERM_DEBUG_ON (update, UPDATE_PREFIX);
      vty_out (vty, "BGP updates debugging is on for %s%s", argv[0], VTY_NEWLINE);
    }

  return CMD_SUCCESS;
}

DEFUN (no_debug_bgp_update_prefix,
       no_debug_bgp_update_prefix_cmd,
       "no debug bgp updates prefix (A.B.C.D/M|X:X::X:X/M)",
       NO_STR
       DEBUG_STR
       BGP_STR
       "BGP updates\n"
       "Specify a prefix to debug\n"
       "IP prefix <network>/<length>, e.g., 35.0.0.0/8\n"
       "IPv6 prefix <network>/<length>\n")

{
  struct prefix *argv_p;
  int found_prefix = 0;
  int ret;

  argv_p = prefix_new();
  ret = str2prefix (argv[0], argv_p);
  if (!ret)
    {
      prefix_free(argv_p);
      vty_out (vty, "%% Malformed Prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (bgp_debug_update_prefixes && !list_isempty(bgp_debug_update_prefixes))
    {
      found_prefix = bgp_debug_list_remove_entry(bgp_debug_update_prefixes, NULL, argv_p);

      if (list_isempty(bgp_debug_update_prefixes))
        {
          if (vty->node == CONFIG_NODE)
            {
              DEBUG_OFF (update, UPDATE_PREFIX);
            }
          else
            {
              TERM_DEBUG_OFF (update, UPDATE_PREFIX);
              vty_out (vty, "BGP updates debugging (per prefix) is off%s", VTY_NEWLINE);
            }
        }
    }

  if (found_prefix)
    vty_out (vty, "BGP updates debugging is off for %s%s", argv[0], VTY_NEWLINE);
  else
    vty_out (vty, "BGP updates debugging was not enabled for %s%s", argv[0], VTY_NEWLINE);

  return CMD_SUCCESS;
}

DEFUN (no_debug_bgp_update,
       no_debug_bgp_update_cmd,
       "no debug bgp updates",
       NO_STR
       DEBUG_STR
       BGP_STR
       "BGP updates\n")
{
  bgp_debug_list_free(bgp_debug_update_in_peers);
  bgp_debug_list_free(bgp_debug_update_out_peers);
  bgp_debug_list_free(bgp_debug_update_prefixes);

  if (vty->node == CONFIG_NODE)
    {
      DEBUG_OFF (update, UPDATE_IN);
      DEBUG_OFF (update, UPDATE_OUT);
      DEBUG_OFF (update, UPDATE_PREFIX);
    }
  else
    {
      TERM_DEBUG_OFF (update, UPDATE_IN);
      TERM_DEBUG_OFF (update, UPDATE_OUT);
      TERM_DEBUG_OFF (update, UPDATE_PREFIX);
      vty_out (vty, "BGP updates debugging is off%s", VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

/* debug bgp zebra */
DEFUN (debug_bgp_zebra,
       debug_bgp_zebra_cmd,
       "debug bgp zebra",
       DEBUG_STR
       BGP_STR
       "BGP Zebra messages\n")
{
  if (vty->node == CONFIG_NODE)
    DEBUG_ON (zebra, ZEBRA);
  else
    {
      TERM_DEBUG_ON (zebra, ZEBRA);
      vty_out (vty, "BGP zebra debugging is on%s", VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

DEFUN (debug_bgp_zebra_prefix,
       debug_bgp_zebra_prefix_cmd,
       "debug bgp zebra prefix (A.B.C.D/M|X:X::X:X/M)",
       DEBUG_STR
       BGP_STR
       "BGP Zebra messages\n"
       "Specify a prefix to debug\n"
       "IP prefix <network>/<length>, e.g., 35.0.0.0/8\n"
       "IPv6 prefix <network>/<length>\n")

{
  struct prefix *argv_p;
  int ret;

  argv_p = prefix_new();
  ret = str2prefix (argv[0], argv_p);
  if (!ret)
    {
      prefix_free(argv_p);
      vty_out (vty, "%% Malformed Prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (!bgp_debug_zebra_prefixes)
    bgp_debug_zebra_prefixes = list_new();

  if (bgp_debug_list_has_entry(bgp_debug_zebra_prefixes, NULL, argv_p))
    {
      vty_out (vty, "BGP zebra debugging is already enabled for %s%s", argv[0], VTY_NEWLINE);
      return CMD_SUCCESS;
    }

  bgp_debug_list_add_entry(bgp_debug_zebra_prefixes, NULL, argv_p);

  if (vty->node == CONFIG_NODE)
    DEBUG_ON (zebra, ZEBRA);
  else
    {
      TERM_DEBUG_ON (zebra, ZEBRA);
      vty_out (vty, "BGP zebra debugging is on for %s%s", argv[0], VTY_NEWLINE);
    }

  return CMD_SUCCESS;
}

DEFUN (no_debug_bgp_zebra,
       no_debug_bgp_zebra_cmd,
       "no debug bgp zebra",
       NO_STR
       DEBUG_STR
       BGP_STR
       "BGP Zebra messages\n")
{
  bgp_debug_list_free(bgp_debug_zebra_prefixes);

  if (vty->node == CONFIG_NODE)
    DEBUG_OFF (zebra, ZEBRA);
  else
    {
      TERM_DEBUG_OFF (zebra, ZEBRA);
      vty_out (vty, "BGP zebra debugging is off%s", VTY_NEWLINE);
    }
  return CMD_SUCCESS;
}

DEFUN (no_debug_bgp_zebra_prefix,
       no_debug_bgp_zebra_prefix_cmd,
       "no debug bgp zebra prefix (A.B.C.D/M|X:X::X:X/M)",
       NO_STR
       DEBUG_STR
       BGP_STR
       "BGP Zebra messages\n"
       "Specify a prefix to debug\n"
       "IP prefix <network>/<length>, e.g., 35.0.0.0/8\n"
       "IPv6 prefix <network>/<length>\n")

{
  struct prefix *argv_p;
  int found_prefix = 0;
  int ret;

  argv_p = prefix_new();
  ret = str2prefix (argv[0], argv_p);
  if (!ret)
    {
      prefix_free(argv_p);
      vty_out (vty, "%% Malformed Prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (bgp_debug_zebra_prefixes && !list_isempty(bgp_debug_zebra_prefixes))
    {
      found_prefix = bgp_debug_list_remove_entry(bgp_debug_neighbor_events_peers, NULL, argv_p);

      if (list_isempty(bgp_debug_zebra_prefixes))
        {
          if (vty->node == CONFIG_NODE)
            DEBUG_OFF (zebra, ZEBRA);
          else
            {
              TERM_DEBUG_OFF (zebra, ZEBRA);
              vty_out (vty, "BGP zebra debugging is off%s", VTY_NEWLINE);
            }
        }
    }

  if (found_prefix)
    vty_out (vty, "BGP zebra debugging is off for %s%s", argv[0], VTY_NEWLINE);
  else
    vty_out (vty, "BGP zebra debugging was not enabled for %s%s", argv[0], VTY_NEWLINE);

  return CMD_SUCCESS;
}

DEFUN (no_debug_bgp,
       no_debug_bgp_cmd,
       "no debug bgp",
       NO_STR
       DEBUG_STR
       BGP_STR)
{
  bgp_debug_list_free(bgp_debug_neighbor_events_peers);
  bgp_debug_list_free(bgp_debug_keepalive_peers);
  bgp_debug_list_free(bgp_debug_update_in_peers);
  bgp_debug_list_free(bgp_debug_update_out_peers);
  bgp_debug_list_free(bgp_debug_update_prefixes);
  bgp_debug_list_free(bgp_debug_zebra_prefixes);

  TERM_DEBUG_OFF (keepalive, KEEPALIVE);
  TERM_DEBUG_OFF (update, UPDATE_IN);
  TERM_DEBUG_OFF (update, UPDATE_OUT);
  TERM_DEBUG_OFF (update, UPDATE_PREFIX);
  TERM_DEBUG_OFF (as4, AS4);
  TERM_DEBUG_OFF (as4, AS4_SEGMENT);
  TERM_DEBUG_OFF (neighbor_events, NEIGHBOR_EVENTS);
  TERM_DEBUG_OFF (zebra, ZEBRA);
  vty_out (vty, "All possible debugging has been turned off%s", VTY_NEWLINE);
      
  return CMD_SUCCESS;
}

DEFUN (show_debugging_bgp,
       show_debugging_bgp_cmd,
       "show debugging bgp",
       SHOW_STR
       DEBUG_STR
       BGP_STR)
{
  vty_out (vty, "BGP debugging status:%s", VTY_NEWLINE);

  if (BGP_DEBUG (as4, AS4))
    vty_out (vty, "  BGP as4 debugging is on%s", VTY_NEWLINE);

  if (BGP_DEBUG (as4, AS4_SEGMENT))
    vty_out (vty, "  BGP as4 aspath segment debugging is on%s", VTY_NEWLINE);

  if (BGP_DEBUG (neighbor_events, NEIGHBOR_EVENTS))
    bgp_debug_list_print (vty, "  BGP neighbor-events debugging is on",
                          bgp_debug_neighbor_events_peers);

  if (BGP_DEBUG (keepalive, KEEPALIVE))
    bgp_debug_list_print (vty, "  BGP keepalives debugging is on",
                          bgp_debug_keepalive_peers);

  if (BGP_DEBUG (nht, NHT))
    vty_out (vty, "  BGP next-hop tracking debugging is on%s", VTY_NEWLINE);

  if (BGP_DEBUG (update, UPDATE_PREFIX))
    bgp_debug_list_print (vty, "  BGP updates debugging is on for",
                          bgp_debug_update_prefixes);

  if (BGP_DEBUG (update, UPDATE_IN))
    bgp_debug_list_print (vty, "  BGP updates debugging is on (inbound)",
                          bgp_debug_update_in_peers);

  if (BGP_DEBUG (update, UPDATE_OUT))
    bgp_debug_list_print (vty, "  BGP updates debugging is on (outbound)",
                          bgp_debug_update_out_peers);

  if (BGP_DEBUG (zebra, ZEBRA))
    bgp_debug_list_print (vty, "  BGP zebra debugging is on",
                          bgp_debug_zebra_prefixes);

  vty_out (vty, "%s", VTY_NEWLINE);
  return CMD_SUCCESS;
}

static int
bgp_config_write_debug (struct vty *vty)
{
  int write = 0;

  if (CONF_BGP_DEBUG (as4, AS4))
    {
      vty_out (vty, "debug bgp as4%s", VTY_NEWLINE);
      write++;
    }

  if (CONF_BGP_DEBUG (as4, AS4_SEGMENT))
    {
      vty_out (vty, "debug bgp as4 segment%s", VTY_NEWLINE);
      write++;
    }

  if (CONF_BGP_DEBUG (keepalive, KEEPALIVE))
    {
      vty_out (vty, "debug bgp keepalives%s", VTY_NEWLINE);
      write++;
    }

  if (CONF_BGP_DEBUG (update, UPDATE_IN) && CONF_BGP_DEBUG (update, UPDATE_OUT))
    {
      vty_out (vty, "debug bgp updates%s", VTY_NEWLINE);
      write++;
    }
  else if (CONF_BGP_DEBUG (update, UPDATE_IN))
    {
      vty_out (vty, "debug bgp updates in%s", VTY_NEWLINE);
      write++;
    }
  else if (CONF_BGP_DEBUG (update, UPDATE_OUT))
    {
      vty_out (vty, "debug bgp updates out%s", VTY_NEWLINE);
      write++;
    }

  if (CONF_BGP_DEBUG (neighbor_events, NEIGHBOR_EVENTS))
    {
      vty_out (vty, "debug bgp neighbor-events%s", VTY_NEWLINE);
      write++;
    }

  if (CONF_BGP_DEBUG (zebra, ZEBRA))
    {
      vty_out (vty, "debug bgp zebra%s", VTY_NEWLINE);
      write++;
    }

    if (CONF_BGP_DEBUG (nht, NHT))
    {
      vty_out (vty, "debug bgp nht%s", VTY_NEWLINE);
      write++;
    }

  return write;
}

static struct cmd_node debug_node =
{
  DEBUG_NODE,
  "",
  1
};

void
bgp_debug_init (void)
{
  install_node (&debug_node, bgp_config_write_debug);

  install_element (ENABLE_NODE, &show_debugging_bgp_cmd);

  install_element (ENABLE_NODE, &debug_bgp_as4_cmd);
  install_element (CONFIG_NODE, &debug_bgp_as4_cmd);
  install_element (ENABLE_NODE, &debug_bgp_as4_segment_cmd);
  install_element (CONFIG_NODE, &debug_bgp_as4_segment_cmd);

  install_element (ENABLE_NODE, &debug_bgp_neighbor_events_cmd);
  install_element (CONFIG_NODE, &debug_bgp_neighbor_events_cmd);
  install_element (ENABLE_NODE, &debug_bgp_nht_cmd);
  install_element (CONFIG_NODE, &debug_bgp_nht_cmd);
  install_element (ENABLE_NODE, &debug_bgp_keepalive_cmd);
  install_element (CONFIG_NODE, &debug_bgp_keepalive_cmd);
  install_element (ENABLE_NODE, &debug_bgp_update_cmd);
  install_element (CONFIG_NODE, &debug_bgp_update_cmd);
  install_element (ENABLE_NODE, &debug_bgp_update_direct_cmd);
  install_element (CONFIG_NODE, &debug_bgp_update_direct_cmd);
  install_element (ENABLE_NODE, &debug_bgp_zebra_cmd);
  install_element (CONFIG_NODE, &debug_bgp_zebra_cmd);

  /* deb bgp updates [in|out] A.B.C.D */
  install_element (ENABLE_NODE, &debug_bgp_update_direct_peer_cmd);
  install_element (CONFIG_NODE, &debug_bgp_update_direct_peer_cmd);
  install_element (ENABLE_NODE, &no_debug_bgp_update_direct_peer_cmd);
  install_element (CONFIG_NODE, &no_debug_bgp_update_direct_peer_cmd);

  /* deb bgp updates prefix A.B.C.D/M */
  install_element (ENABLE_NODE, &debug_bgp_update_prefix_cmd);
  install_element (CONFIG_NODE, &debug_bgp_update_prefix_cmd);
  install_element (ENABLE_NODE, &no_debug_bgp_update_prefix_cmd);
  install_element (CONFIG_NODE, &no_debug_bgp_update_prefix_cmd);

  /* deb bgp zebra prefix A.B.C.D/M */
  install_element (ENABLE_NODE, &debug_bgp_zebra_prefix_cmd);
  install_element (CONFIG_NODE, &debug_bgp_zebra_prefix_cmd);
  install_element (ENABLE_NODE, &no_debug_bgp_zebra_prefix_cmd);
  install_element (CONFIG_NODE, &no_debug_bgp_zebra_prefix_cmd);

  install_element (ENABLE_NODE, &no_debug_bgp_as4_cmd);
  install_element (CONFIG_NODE, &no_debug_bgp_as4_cmd);
  install_element (ENABLE_NODE, &no_debug_bgp_as4_segment_cmd);
  install_element (CONFIG_NODE, &no_debug_bgp_as4_segment_cmd);

  /* deb bgp neighbor-events A.B.C.D */
  install_element (ENABLE_NODE, &debug_bgp_neighbor_events_peer_cmd);
  install_element (CONFIG_NODE, &debug_bgp_neighbor_events_peer_cmd);
  install_element (ENABLE_NODE, &no_debug_bgp_neighbor_events_peer_cmd);
  install_element (CONFIG_NODE, &no_debug_bgp_neighbor_events_peer_cmd);

  /* deb bgp keepalive A.B.C.D */
  install_element (ENABLE_NODE, &debug_bgp_keepalive_peer_cmd);
  install_element (CONFIG_NODE, &debug_bgp_keepalive_peer_cmd);
  install_element (ENABLE_NODE, &no_debug_bgp_keepalive_peer_cmd);
  install_element (CONFIG_NODE, &no_debug_bgp_keepalive_peer_cmd);

  install_element (ENABLE_NODE, &no_debug_bgp_neighbor_events_cmd);
  install_element (CONFIG_NODE, &no_debug_bgp_neighbor_events_cmd);
  install_element (ENABLE_NODE, &no_debug_bgp_nht_cmd);
  install_element (CONFIG_NODE, &no_debug_bgp_nht_cmd);
  install_element (ENABLE_NODE, &no_debug_bgp_keepalive_cmd);
  install_element (CONFIG_NODE, &no_debug_bgp_keepalive_cmd);
  install_element (ENABLE_NODE, &no_debug_bgp_update_cmd);
  install_element (CONFIG_NODE, &no_debug_bgp_update_cmd);
  install_element (ENABLE_NODE, &no_debug_bgp_zebra_cmd);
  install_element (CONFIG_NODE, &no_debug_bgp_zebra_cmd);
  install_element (ENABLE_NODE, &no_debug_bgp_cmd);
}

/* Return true if this prefix is on the per_prefix_list of prefixes to debug
 * for BGP_DEBUG_TYPE
 */
static int
bgp_debug_per_prefix (struct prefix *p, unsigned long term_bgp_debug_type,
                      unsigned int BGP_DEBUG_TYPE, struct list *per_prefix_list)
{
  struct bgp_debug_filter *filter;
  struct listnode *node, *nnode;

  if (term_bgp_debug_type & BGP_DEBUG_TYPE)
    {
      /* We are debugging all prefixes so return true */
      if (!per_prefix_list || list_isempty(per_prefix_list))
        return 1;

      else
        {
          if (!p)
            return 0;

          for (ALL_LIST_ELEMENTS (per_prefix_list, node, nnode, filter))
            if (filter->p->prefixlen == p->prefixlen && prefix_match(filter->p, p))
              return 1;

          return 0;
        }
    }

  return 0;
}

/* Return true if this peer is on the per_peer_list of peers to debug
 * for BGP_DEBUG_TYPE
 */
static int
bgp_debug_per_peer(struct peer *peer, unsigned long term_bgp_debug_type,
                   unsigned int BGP_DEBUG_TYPE, struct list *per_peer_list)
{
  struct bgp_debug_filter *filter;
  struct listnode *node, *nnode;

  if (term_bgp_debug_type & BGP_DEBUG_TYPE)
    {
      /* We are debugging all peers so return true */
      if (!per_peer_list || list_isempty(per_peer_list))
        return 1;

      else
        {
          if (!peer)
            return 0;

          for (ALL_LIST_ELEMENTS (per_peer_list, node, nnode, filter))
            if (filter->peer == peer)
              return 1;

          return 0;
        }
    }

  return 0;
}

int
bgp_debug_neighbor_events (struct peer *peer)
{
  return bgp_debug_per_peer (peer,
                             term_bgp_debug_neighbor_events,
                             BGP_DEBUG_NEIGHBOR_EVENTS,
                             bgp_debug_neighbor_events_peers);
}

int
bgp_debug_keepalive (struct peer *peer)
{
  return bgp_debug_per_peer (peer,
                             term_bgp_debug_keepalive,
                             BGP_DEBUG_KEEPALIVE,
                             bgp_debug_keepalive_peers);
}

int
bgp_debug_update (struct peer *peer, struct prefix *p, unsigned int inbound)
{
  if (inbound)
    {
      if (bgp_debug_per_peer (peer, term_bgp_debug_update, BGP_DEBUG_UPDATE_IN,
                              bgp_debug_update_in_peers))
        return 1;
    }

  /* outbound */
  else
    {
      if (bgp_debug_per_peer (peer, term_bgp_debug_update,
                              BGP_DEBUG_UPDATE_OUT,
                              bgp_debug_update_out_peers))
        return 1;
    }


  if (BGP_DEBUG (update, UPDATE_PREFIX))
    {
      if (bgp_debug_per_prefix (p, term_bgp_debug_update,
                                BGP_DEBUG_UPDATE_PREFIX,
                                bgp_debug_update_prefixes))
        return 1;
    }

  return 0;
}

int
bgp_debug_zebra (struct prefix *p)
{
  if (BGP_DEBUG (zebra, ZEBRA))
    {
      if (bgp_debug_per_prefix (p, term_bgp_debug_zebra, BGP_DEBUG_ZEBRA,
                                bgp_debug_zebra_prefixes))
        return 1;
    }

  return 0;
}
