/* Copyright (c) 2008, 2014 The Board of Trustees of The Leland Stanford
* Junior University
* Copyright (c) 2011, 2012 Open Networking Foundation
*
* We are making the OpenFlow specification and associated documentation
* (Software) available for public use and benefit with the expectation
* that others will use, modify and enhance the Software and contribute
* those enhancements back to the community. However, since we would
* like to make the Software available for broadest use, with as few
* restrictions as possible permission is hereby granted, free of
* charge, to any person obtaining a copy of this Software to deal in
* the Software under the copyrights without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
* BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
* ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* The name and trademarks of copyright holder(s) may NOT be used in
* advertising or publicity pertaining to the Software or any
* derivatives without specific, written prior permission.
*/

/* OpenFlow: protocol between controller and datapath. */

#ifndef OPENFLOW_15_H
#define OPENFLOW_15_H 1

#include "openflow/openflow-1.4.h"


/* ## ---------- ## */
/* ## ofp15_port ## */
/* ## ---------- ## */

/* Ingress or egress pipeline fields. */
struct ofp15_port_desc_prop_oxm {
    ovs_be16         type;    /* One of OFPPDPT15_PIPELINE_INPUT or
                                 OFPPDPT15_PIPELINE_OUTPUT. */
    ovs_be16         length;  /* Length in bytes of this property. */
    /* Followed by:
     *   - Exactly (length - 4) bytes containing the oxm_ids, then
     *   - Exactly (length + 7)/8*8 - (length) (between 0 and 7)
     *     bytes of all-zero bytes */
    ovs_be32         oxm_ids[0];   /* Array of OXM headers */
};
OFP_ASSERT(sizeof(struct ofp15_port_desc_prop_oxm) == 4);

/* Recirculate port description property. */
struct ofp15_port_desc_prop_recirculate {
    ovs_be16     type;          /* OFPPDPT15_RECIRCULATE. */
    ovs_be16     length;        /* Length in bytes of the property,
                                   including this header, excluding padding. */
    /* Followed by:
     *   - Exactly (length - 4) bytes containing the port numbers, then
     *   - Exactly (length + 7)/8*8 - (length) (between 0 and 7)
     *     bytes of all-zero bytes */
    ovs_be32     port_nos[0];    /* List of recirculated input port numbers.
                                    0 or more. The number of port numbers
                                    is inferred from the length field in
                                    the header. */
};
OFP_ASSERT(sizeof(struct ofp15_port_desc_prop_recirculate) == 4);

#endif /* openflow/openflow-1.4.h */
