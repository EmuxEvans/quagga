#include <zebra.h>
#include <vty.h>

#include "zebra/rib.h"
#include "zebra/zebra_rnh.h"

int zebra_evaluate_rnh_table (vrf_id_t vrfid, int family, int force)
{ return 0; }

void zebra_print_rnh_table (vrf_id_t vrfid, int family, struct vty *vty)
{}

void zebra_register_rnh_static_nh(struct prefix *p, struct route_node *rn)
{}

void zebra_deregister_rnh_static_nh(struct prefix *p, struct route_node *rn)
{}

void zebra_deregister_rnh_static_nexthops (struct nexthop *nexthop, struct route_node *rn)
{}
