/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cofbclist.h"

using namespace rofl;

cofbclist::cofbclist(
		uint8_t ofp_version,
		int bcnum) :
				ofp_version(ofp_version)
{
	for (int i = 0; i < bcnum; ++i) {
		elems[i] = cofbucket(ofp_version);
	}
}



cofbclist::~cofbclist()
{

}



cofbclist::cofbclist(cofbclist const& bclist)
{
	*this = bclist;
}



cofbclist&
cofbclist::operator= (cofbclist const& bclist)
{
	if (this == &bclist)
		return *this;

	this->ofp_version = bclist.ofp_version;
	coflist<cofbucket>::operator= (bclist);

	return *this;
}



std::vector<cofbucket>&
cofbclist::unpack(
	uint8_t* buckets,
	size_t bclen)
throw (eBucketBadLen, eBadActionBadOutPort)
{
	switch (ofp_version) {
	case OFP12_VERSION: {
		return unpack((struct ofp12_bucket*)buckets, bclen);
	} break;
	case OFP13_VERSION: {
		return unpack((struct ofp13_bucket*)buckets, bclen);
	} break;
	default:
		throw eBadVersion();
	}
}




uint8_t*
cofbclist::pack(
	uint8_t* buckets,
	size_t bclen) const
throw (eBcListInval)
{
	switch (ofp_version) {
	case OFP12_VERSION: {
		return (uint8_t*)pack((struct ofp12_bucket*)buckets, bclen);
	} break;
	case OFP13_VERSION: {
		return (uint8_t*)pack((struct ofp13_bucket*)buckets, bclen);
	} break;
	default:
		throw eBadVersion();
	}
}



std::vector<cofbucket>&
cofbclist::unpack(
		struct ofp12_bucket *buckets,
		size_t bclen)
throw (eBucketBadLen, eBadActionBadOutPort)
{
	clear(); // clears elems

	// sanity check: bclen must be of size at least of ofp_bucket
	if (bclen < (int)sizeof(struct ofp12_bucket))
		return elems;

	// first bucket
	struct ofp12_bucket *bchdr = buckets;


	while (bclen > 0)
	{
		if (be16toh(bchdr->len) < sizeof(struct ofp12_bucket))
			throw eBucketBadLen();

		cofbucket bucket(ofp_version, (uint8_t*)bchdr, be16toh(bchdr->len));

		next() = cofbucket(ofp_version, (uint8_t*)bchdr, be16toh(bchdr->len) );

		bclen -= be16toh(bchdr->len);
		bchdr = (struct ofp12_bucket*)(((uint8_t*)bchdr) + be16toh(bchdr->len));
	}

	return elems;
}


struct ofp12_bucket*
cofbclist::pack(
	struct ofp12_bucket *buckets,
	size_t bclen) const throw (eBcListInval)
{
	size_t needed_bclen = length();

	if (bclen < needed_bclen)
		throw eBcListInval();

	struct ofp12_bucket *bchdr = buckets; // first bucket header

	cofbclist::const_iterator it;
	for (it = elems.begin(); it != elems.end(); ++it)
	{
		cofbucket const& bucket = (*it);

		bchdr = (struct ofp12_bucket*)
				((uint8_t*)(bucket.pack((uint8_t*)bchdr, bucket.length())) + bucket.length());
	}

	return buckets;
}



std::vector<cofbucket>&
cofbclist::unpack(
		struct ofp13_bucket *buckets,
		size_t bclen)
throw (eBucketBadLen, eBadActionBadOutPort)
{
	clear(); // clears elems

	// sanity check: bclen must be of size at least of ofp_bucket
	if (bclen < (int)sizeof(struct ofp13_bucket))
		return elems;

	// first bucket
	struct ofp13_bucket *bchdr = buckets;


	while (bclen > 0)
	{
		if (be16toh(bchdr->len) < sizeof(struct ofp13_bucket))
			throw eBucketBadLen();

		cofbucket bucket(ofp_version, (uint8_t*)bchdr, be16toh(bchdr->len));

		next() = cofbucket(ofp_version, (uint8_t*)bchdr, be16toh(bchdr->len) );

		bclen -= be16toh(bchdr->len);
		bchdr = (struct ofp13_bucket*)(((uint8_t*)bchdr) + be16toh(bchdr->len));
	}

	return elems;
}


struct ofp13_bucket*
cofbclist::pack(
	struct ofp13_bucket *buckets,
	size_t bclen) const throw (eBcListInval)
{
	size_t needed_bclen = length();

	if (bclen < needed_bclen)
		throw eBcListInval();

	struct ofp13_bucket *bchdr = buckets; // first bucket header

	cofbclist::const_iterator it;
	for (it = elems.begin(); it != elems.end(); ++it)
	{
		cofbucket const& bucket = (*it);

		bchdr = (struct ofp13_bucket*)
				((uint8_t*)(bucket.pack((uint8_t*)bchdr, bucket.length())) + bucket.length());
	}

	return buckets;
}



size_t
cofbclist::length() const
{
	size_t len = 0;
	cofbclist::const_iterator it;
	for (it = elems.begin(); it != elems.end(); ++it)
	{
		len += (*it).length();
	}
	return len;
}

