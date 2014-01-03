/*
 * openflow.h
 *
 *  Created on: 02.03.2013
 *      Author: andi
 */

#ifndef OPENFLOW_COMMON_H_
#define OPENFLOW_COMMON_H_ 1

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#ifdef SWIG
#define OFP_ASSERT(EXPR)        /* SWIG can't handle OFP10_ASSERT. */
#elif !defined(__cplusplus)
/* Build-time assertion for use in a declaration context. */
#define OFP_ASSERT(EXPR)                                                \
        extern int (*build_assert(void))[ sizeof(struct {               \
                    unsigned int build_assert_failed : (EXPR) ? 1 : -1; })]
#else /* __cplusplus */
#define OFP_ASSERT(_EXPR) typedef int build_assert_failed[(_EXPR) ? 1 : -1]
#endif /* __cplusplus */

#ifndef SWIG
#define OFP_PACKED __attribute__((packed))
#else
#define OFP_PACKED              /* SWIG doesn't understand __attribute. */
#endif


namespace rofl {

#define OFP_MAX_TABLE_NAME_LEN 32
#define OFP_MAX_PORT_NAME_LEN  16

#define OFP_TCP_PORT  6633
#define OFP_SSL_PORT  6633

#define OFP_VERSION_UNKNOWN 0

#define OFP_ETH_ALEN 6          /* Bytes in an Ethernet address. */

namespace openflow {

	/* Header on all OpenFlow packets. */
	struct ofp_header {
		uint8_t version;    /* OFP10_VERSION. */
		uint8_t type;       /* One of the OFP10T_ constants. */
		uint16_t length;    /* Length including this ofp10_header. */
		uint32_t xid;       /* Transaction id associated with this packet.
							   Replies use the same id as was in the request
							   to facilitate pairing. */
	};
	OFP_ASSERT(sizeof(struct ofp_header) == 8);

	enum ofp_type {
		/* Immutable messages. */
		OFPT_HELLO 					= 0,    /* Symmetric message */
		OFPT_ERROR 					= 1,	/* Symmetric message */
		OFPT_ECHO_REQUEST 			= 2,	/* Symmetric message */
		OFPT_ECHO_REPLY				= 3,    /* Symmetric message */
		OFPT_EXPERIMENTER			= 4,    /* Symmetric message */

		/* Switch configuration messages. */
		OFPT_FEATURES_REQUEST		= 5,    /* Controller/switch message */
		OFPT_FEATURES_REPLY			= 6,    /* Controller/switch message */
		OFPT_GET_CONFIG_REQUEST		= 7,    /* Controller/switch message */
		OFPT_GET_CONFIG_REPLY		= 8,    /* Controller/switch message */
		OFPT_SET_CONFIG				= 9,    /* Controller/switch message */

		/* Asynchronous messages. */
		OFPT_PACKET_IN				= 10,   /* Async message */
		OFPT_FLOW_REMOVED			= 11,   /* Async message */
		OFPT_PORT_STATUS			= 12,   /* Async message */

		/* Controller command messages. */
		OFPT_PACKET_OUT				= 13,   /* Controller/switch message */
		OFPT_FLOW_MOD				= 14,   /* Controller/switch message */
		OFPT_GROUP_MOD				= 15,   /* Controller/switch message */
		OFPT_PORT_MOD				= 16,   /* Controller/switch message */
		OFPT_TABLE_MOD				= 17,   /* Controller/switch message */

		/* Multipart messages. */
		OFPT_MULTIPART_REQUEST		= 18,   /* Controller/switch message */
		OFPT_MULTIPART_REPLY		= 19,   /* Controller/switch message */
		OFPT_STATS_REQUEST			= 18,   /* Controller/switch message */
		OFPT_STATS__REPLY			= 19,   /* Controller/switch message */

		/* Barrier messages. */
		OFPT_BARRIER_REQUEST		= 20,   /* Controller/switch message */
		OFPT_BARRIER_REPLY			= 21,   /* Controller/switch message */

		/* Queue Configuration messages. */
		OFPT_QUEUE_GET_CONFIG_REQUEST	= 22,  /* Controller/switch message */
		OFPT_QUEUE_GET_CONFIG_REPLY		= 23,  /* Controller/switch message */

		/* Controller role change request messages. */
		OFPT_ROLE_REQUEST    		= 24, /* Controller/switch message */
		OFPT_ROLE_REPLY				= 25, /* Controller/switch message */

		/* Asynchronous message configuration. */
		OFPT_GET_ASYNC_REQUEST		= 26, /* Controller/switch message */
		OFPT_GET_ASYNC_REPLY		= 27, /* Controller/switch message */
		OFPT_SET_ASYNC				= 28, /* Controller/switch message */

		/* Meters and rate limiters configuration messages. */
		OFPT_METER_MOD				= 29, /* Controller/switch message */
	};

	/* OFPT_ERROR: Error message (datapath -> controller). */
	struct ofp_error_msg {
		struct ofp_header header;

		uint16_t type;
		uint16_t code;
		uint8_t data[0];          /* Variable-length data.  Interpreted based
									 on the type and code.  No padding. */
	};
	OFP_ASSERT(sizeof(struct ofp_error_msg) == 12);


	enum ofp_flow_mod_command {
		OFPFC_ADD,              /* New flow. */
		OFPFC_MODIFY,           /* Modify all matching flows. */
		OFPFC_MODIFY_STRICT,    /* Modify entry strictly matching wildcards and
								   priority. */
		OFPFC_DELETE,           /* Delete all matching flows. */
		OFPFC_DELETE_STRICT     /* Delete entry strictly matching wildcards and
								   priority. */
	};


	/* What changed about the physical port */
	enum ofp_port_reason {
		OFPPR_ADD,              /* The port was added. */
		OFPPR_DELETE,           /* The port was removed. */
		OFPPR_MODIFY            /* Some attribute of the port has changed. */
	};


	enum ofp_instruction_type {
		OFPIT_GOTO_TABLE = 1,       /* Setup the next table in the lookup
									   pipeline */
		OFPIT_WRITE_METADATA = 2,   /* Setup the metadata field for use later in
									   pipeline */
		OFPIT_WRITE_ACTIONS = 3,    /* Write the action(s) onto the datapath action
									   set */
		OFPIT_APPLY_ACTIONS = 4,    /* Applies the action(s) immediately */
		OFPIT_CLEAR_ACTIONS = 5,    /* Clears all actions from the datapath
									   action set */
		OFPIT_METER = 6,				/* Apply meter (rate limiter) */
		OFPIT_EXPERIMENTER = 0xFFFF  /* Experimenter instruction */
	};


	/* Generic ofp_instruction structure */
	struct ofp_instruction {
		uint16_t type;                /* Instruction type */
		uint16_t len;                 /* Length of this struct in bytes. */
		uint8_t pad[4];               /* Align to 64-bits */
	};
	OFP_ASSERT(sizeof(struct ofp_instruction) == 8);




	/* Action header that is common to all actions.  The length includes the
	 * header and any padding used to make the action 64-bit aligned.
	 * NB: The length of an action *must* always be a multiple of eight. */
	struct ofp_action_header {
		uint16_t type;                  /* One of OFPAT_*. */
		uint16_t len;                   /* Length of action, including this
										   header.  This is the length of action,
										   including any padding to make it
										   64-bit aligned. */
		uint8_t pad[4];
	};
	OFP_ASSERT(sizeof(struct ofp_action_header) == 8);

	enum ofp_action_type {
		OFPAT_OUTPUT 			= 0, 	/* Output to switch port. */
		// OF1.0 only actions
		OFPAT_SET_VLAN_VID      = 1, 	/* Set the 802.1q VLAN id. */
		OFPAT_SET_VLAN_PCP		= 2,    /* Set the 802.1q priority. */
		OFPAT_STRIP_VLAN		= 3,    /* Strip the 802.1q header. */
		OFPAT_SET_DL_SRC		= 4,    /* Ethernet source address. */
		OFPAT_SET_DL_DST		= 5,    /* Ethernet destination address. */
		OFPAT_SET_NW_SRC		= 6,    /* IP source address. */
		OFPAT_SET_NW_DST		= 7,    /* IP destination address. */
		OFPAT_SET_NW_TOS		= 8,    /* IP ToS (DSCP field, 6 bits). */
		OFPAT_SET_TP_SRC		= 9,    /* TCP/UDP source port. */
		OFPAT_SET_TP_DST		= 10,   /* TCP/UDP destination port. */
		// OF1.0 only actions (end)
		// Please note: #0 and #11 needs special care in OF10 and OF12
		OFPAT_COPY_TTL_OUT 		= 11, 	/* Copy TTL "outwards" -- from next-to-outermost to outermost */
		OFPAT_COPY_TTL_IN 		= 12, 	/* Copy TTL "inwards" -- from outermost to next-to-outermost */
		OFPAT_SET_MPLS_TTL 		= 15, 	/* MPLS TTL */
		OFPAT_DEC_MPLS_TTL 		= 16, 	/* Decrement MPLS TTL */
		OFPAT_PUSH_VLAN 		= 17, 	/* Push a new VLAN tag */
		OFPAT_POP_VLAN 			= 18, 	/* Pop the outer VLAN tag */
		OFPAT_PUSH_MPLS 		= 19, 	/* Push a new MPLS tag */
		OFPAT_POP_MPLS 			= 20, 	/* Pop the outer MPLS tag */
		OFPAT_SET_QUEUE 		= 21, 	/* Set queue id when outputting to a port */
		OFPAT_GROUP 			= 22, 	/* Apply group. */
		OFPAT_SET_NW_TTL 		= 23, 	/* IP TTL. */
		OFPAT_DEC_NW_TTL 		= 24, 	/* Decrement IP TTL. */
		OFPAT_SET_FIELD 		= 25, 	/* Set a header field using OXM TLV format. */
		OFPAT_EXPERIMENTER		= 0xffff
	};



	struct ofp_oxm_hdr {
		uint16_t oxm_class;		/* oxm_class */
		uint8_t  oxm_field;		/* includes has_mask bit! */
		uint8_t  oxm_length;	/* oxm_length */
	};



	// OXM_OF_VLAN_PCP 		/* 3 bits */
	// OXM_OF_IP_DSCP 		/* 6 bits */
	// OXM_OF_IP_ECN		/* 2 bits */
	// OXM_OF_IP_PROTO		/* 8 bits */
	// OXM_OF_ICMPV4_TYPE
	// OXM_OF_ICMPV4_CODE
	// OXM_OF_ICMPV6_TYPE
	// OXM_OF_ICMPV6_CODE
	// OXM_OF_MPLS_TC		/* 3 bits */
	struct ofp_oxm_ofb_uint8_t {
		struct ofp_oxm_hdr hdr;		/* oxm header */
		uint8_t byte;
		uint8_t mask;
	};


	// OXM_OF_ETH_TYPE
	// OXM_OF_VLAN_VID (mask)
	// OXM_OF_TCP_SRC
	// OXM_OF_TCP_DST
	// OXM_OF_UDP_SRC
	// OXM_OF_UDP_DST
	// OXM_OF_SCTP_SRC
	// OXM_OF_SCTP_DST
	// OXM_OF_ARP_OP
	struct ofp_oxm_ofb_uint16_t {
		struct ofp_oxm_hdr hdr;		/* oxm header */
		uint16_t word;				/* network byte order */
		uint16_t mask;
	};


	// OXM_OF_IN_PORT
	// OXM_OF_IN_PHY_PORT
	// OXM_OF_IPV4_SRC (mask)
	// OXM_OF_IPV4_DST (mask)
	// OXM_OF_ARP_SPA (mask)
	// OXM_OF_ARP_THA (mask)
	// OXM_OF_IPV6_FLABEL (mask)
	// OXM_OF_MPLS_LABEL
	struct ofp_oxm_ofb_uint32_t {
		struct ofp_oxm_hdr hdr;		/* oxm header */
		uint32_t dword;				/* network byte order */
		uint32_t mask;				/* only valid, when oxm_hasmask=1 */
	};


	// OXM_OF_IPV6_ND_SLL
	// OXM_OF_IPV6_ND_TLL
	struct ofp_oxm_ofb_uint48_t {
		struct ofp_oxm_hdr hdr;		/* oxm header */
		uint8_t value[6];
		uint8_t mask[6];			/* only valid, when oxm_hasmask=1 */
	};

	// OXM_OF_METADATA (mask)
	struct ofp_oxm_ofb_uint64_t {
		struct ofp_oxm_hdr hdr;		/* oxm header */
		uint8_t word[8];
		uint8_t mask[8];
	#if 0
		uint64_t qword;				/* network byte order */
		uint64_t mask;				/* only valid, when oxm_hasmask=1 */
	#endif
	};


	// OXM_OF_ETH_DST (mask)
	// OXM_OF_ETH_SRC (mask)
	struct ofp_oxm_ofb_maddr {
		struct ofp_oxm_hdr hdr;		/* oxm header */
		uint8_t addr[OFP_ETH_ALEN];
		uint8_t mask[OFP_ETH_ALEN]; /* only valid, when oxm_hasmask=1 */
	};


	// OXM_OF_IPV6_SRC (mask)
	// OXM_OF_IPV6_DST (mask)
	// OXM_OF_IPV6_ND_TARGET
	struct ofp_oxm_ofb_ipv6_addr {
		struct ofp_oxm_hdr hdr;		/* oxm header */
		uint8_t addr[16];
		uint8_t mask[16];			/* only valid, when oxm_hasmask=1 */
	};


	/* OXM Class IDs.
	 * The high order bit differentiate reserved classes from member classes.
	 * Classes 0x0000 to 0x7FFF are member classes, allocated by ONF.
	 * Classes 0x8000 to 0xFFFE are reserved classes, reserved for standardisation.
	 */
	enum ofp_oxm_class {
		OFPXMC_NXM_0			= 0x0000, 	/* Backward compatibility with NXM */
		OFPXMC_NXM_1			= 0x0001,	/* Backward compatibility with NXM */
		OFPXMC_OPENFLOW_BASIC	= 0x8000,	/* Basic class for OpenFlow */
		OFPXMC_EXPERIMENTER		= 0xFFFF,	/* Experimenter class */
	};


	/* OXM Flow match field types for OpenFlow basic class. */
	enum oxm_ofb_match_fields {
		OFPXMT_OFB_IN_PORT = 0,			/* Switch input port. */				// required
		OFPXMT_OFB_IN_PHY_PORT = 1,		/* Switch physical input port. */
		OFPXMT_OFB_METADATA = 2,		/* Metadata passed between tables. */
		OFPXMT_OFB_ETH_DST = 3,			/* Ethernet destination address. */		// required
		OFPXMT_OFB_ETH_SRC = 4,			/* Ethernet source address. */			// required
		OFPXMT_OFB_ETH_TYPE = 5,		/* Ethernet frame type. */				// required
		OFPXMT_OFB_VLAN_VID = 6,		/* VLAN id. */
		OFPXMT_OFB_VLAN_PCP = 7,		/* VLAN priority. */
		OFPXMT_OFB_IP_DSCP = 8,			/* IP DSCP (6 bits in ToS field). */
		OFPXMT_OFB_IP_ECN = 9,			/* IP ECN (2 bits in ToS field). */
		OFPXMT_OFB_IP_PROTO = 10,		/* IP protocol. */						// required
		OFPXMT_OFB_IPV4_SRC = 11,		/* IPv4 source address. */				// required
		OFPXMT_OFB_IPV4_DST = 12,		/* IPv4 destination address. */			// required
		OFPXMT_OFB_TCP_SRC = 13,		/* TCP source port. */					// required
		OFPXMT_OFB_TCP_DST = 14,		/* TCP destination port. */				// required
		OFPXMT_OFB_UDP_SRC = 15,		/* UDP source port. */					// required
		OFPXMT_OFB_UDP_DST = 16,		/* UDP destination port. */				// required
		OFPXMT_OFB_SCTP_SRC = 17,		/* SCTP source port. */
		OFPXMT_OFB_SCTP_DST = 18,		/* SCTP destination port. */
		OFPXMT_OFB_ICMPV4_TYPE = 19,	/* ICMP type. */
		OFPXMT_OFB_ICMPV4_CODE = 20,	/* ICMP code. */
		OFPXMT_OFB_ARP_OP = 21,			/* ARP opcode. */
		OFPXMT_OFB_ARP_SPA = 22,		/* ARP source IPv4 address. */
		OFPXMT_OFB_ARP_TPA = 23,		/* ARP target IPv4 address. */
		OFPXMT_OFB_ARP_SHA = 24,		/* ARP source hardware address. */
		OFPXMT_OFB_ARP_THA = 25,		/* ARP target hardware address. */
		OFPXMT_OFB_IPV6_SRC = 26,		/* IPv6 source address. */				// required
		OFPXMT_OFB_IPV6_DST = 27,		/* IPv6 destination address. */			// required
		OFPXMT_OFB_IPV6_FLABEL = 28,	/* IPv6 Flow Label */
		OFPXMT_OFB_ICMPV6_TYPE = 29,	/* ICMPv6 type. */
		OFPXMT_OFB_ICMPV6_CODE = 30,	/* ICMPv6 code. */
		OFPXMT_OFB_IPV6_ND_TARGET = 31,	/* Target address for ND. */
		OFPXMT_OFB_IPV6_ND_SLL = 32,	/* Source link-layer for ND. */
		OFPXMT_OFB_IPV6_ND_TLL = 33,	/* Target link-layer for ND. */
		OFPXMT_OFB_MPLS_LABEL = 34,		/* MPLS label. */
		OFPXMT_OFB_MPLS_TC = 35,		/* MPLS TC. */
		OFPXMT_OFB_MPLS_BOS = 36,		/* MPLS BoS bit. */
		OFPXMT_OFB_PBB_ISID = 37,		/* PBB I-SID. */
		OFPXMT_OFB_TUNNEL_ID = 38,		/* Logical Port Metadata. */
		OFPXMT_OFB_IPV6_EXTHDR = 39,	/* IPv6 Extension Header pseudo-field */
		/* max value */
		OFPXMT_OFB_MAX,
	};


	/* The VLAN id is 12-bits, so we can use the entire 16 bits to indicate
	* special conditions.
	*/
	enum ofp_vlan_id {
		OFPVID_PRESENT = 0x1000, /* Bit that indicate that a VLAN id is set */
		OFPVID_NONE = 0x0000, /* No VLAN id was set. */
	};





	/* Fields to match against flows */
	struct ofp_match {
		uint16_t type;			/* One of OFPMT_* */
		uint16_t length;		/* Length of ofp_match (excluding padding) */
		/* Followed by:
		 * - Exactly (length - 4) (possibly 0) bytes containing OXM TLVs, then
		 * - Exactly ((length + 7)/8*8 - length) (between 0 and 7) bytes of
		 * all-zero bytes
		 * In summary, ofp_match is padded as needed, to make its overall size
		 * a multiple of 8, to preserve alignement in structures using it.
		 */
		uint8_t oxm_fields[4];
		/* OXMs start here - Make compiler happy */
	};
	OFP_ASSERT(sizeof(struct ofp_match) == 8);


	/* The match type indicates the match structure (set of fields that compose the
	* match) in use. The match type is placed in the type field at the beginning
	* of all match structures. The "OpenFlow Extensible Match" type corresponds
	* to OXM TLV format described below and must be supported by all OpenFlow
	* switches. Extensions that define other match types may be published on the
	* ONF wiki. Support for extensions is optional.
	*/
	enum ofp_match_type {
		OFPMT_STANDARD = 0, /* Deprecated. */
		OFPMT_OXM = 1, 		/* OpenFlow Extensible Match */
	};


	/* Group numbering. Groups can use any number up to OFPG_MAX. */
	enum ofp_group {
		/* Last usable group number. */
		OFPG_MAX        = 0xffffff00,

		/* Fake groups. */
		OFPG_ALL        = 0xfffffffc,  /* Represents all groups for group delete
										  commands. */
		OFPG_ANY        = 0xffffffff   /* Wildcard group used only for flow stats
										  requests. Selects all flows regardless of
										  group (including flows with no group).
										  */
	};

	/* Bucket for use in groups. */
	struct ofp_bucket {
		uint16_t len;                   /* Length the bucket in bytes, including
										   this header and any padding to make it
										   64-bit aligned. */
		uint16_t weight;                /* Relative weight of bucket.  Only
										   defined for select groups. */
		uint32_t watch_port;            /* Port whose state affects whether this
										   bucket is live.  Only required for fast
										   failover groups. */
		uint32_t watch_group;           /* Group whose state affects whether this
										   bucket is live.  Only required for fast
										   failover groups. */
		uint8_t pad[4];
		struct ofp_action_header actions[0]; /* The action length is inferred
											   from the length field in the
											   header. */
	};
	OFP_ASSERT(sizeof(struct ofp_bucket) == 16);



}; // end of namespace openflow
}; // end of namespace rofl


#endif /* OPENFLOW_COMMON_H_ */
