/* dnsmasq is Copyright (c) 2000-2025 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* Code to safely remove RRs from a DNS answer */ 

#include "dnsmasq.h"

/* Go through a domain name, find "pointers" and fix them up based on how many bytes
   we've chopped out of the packet, or check they don't point into an elided part.  */
static int check_name(unsigned char **namep, struct dns_header *header, size_t plen, int fixup, unsigned char **rrs, int rr_count)
{
  unsigned char *ansp = *namep;

  while(1)
    {
      unsigned int label_type;
      
      if (!CHECK_LEN(header, ansp, plen, 1))
	return 0;
      
      label_type = (*ansp) & 0xc0;

      if (label_type == 0xc0)
	{
	  /* pointer for compression. */
	  unsigned int offset;
	  int i;
	  unsigned char *p;
	  
	  if (!CHECK_LEN(header, ansp, plen, 2))
	    return 0;

	  offset = ((*ansp++) & 0x3f) << 8;
	  offset |= *ansp++;

	  p = offset + (unsigned char *)header;
	  
	  for (i = 0; i < rr_count; i++)
	    if (p < rrs[i])
	      break;
	    else
	      if (i & 1)
		offset -= rrs[i] - rrs[i-1];

	  /* does the pointer end up in an elided RR? */
	  if (i & 1)
	    return 0;

	  /* No, scale the pointer */
	  if (fixup)
	    {
	      ansp -= 2;
	      *ansp++ = (offset >> 8) | 0xc0;
	      *ansp++ = offset & 0xff;
	    }
	  break;
	}
      else if (label_type == 0x80)
	return 0; /* reserved */
      else if (label_type == 0x40)
	{
	  /* Extended label type */
	  unsigned int count;
	  
	  if (!CHECK_LEN(header, ansp, plen, 2))
	    return 0;
	  
	  if (((*ansp++) & 0x3f) != 1)
	    return 0; /* we only understand bitstrings */
	  
	  count = *(ansp++); /* Bits in bitstring */
	  
	  if (count == 0) /* count == 0 means 256 bits */
	    ansp += 32;
	  else
	    ansp += ((count-1)>>3)+1;
	}
      else
	{ /* label type == 0 Bottom six bits is length */
	  unsigned int len = (*ansp++) & 0x3f;
	  
	  if (!ADD_RDLEN(header, ansp, plen, len))
	    return 0;

	  if (len == 0)
	    break; /* zero length label marks the end. */
	}
    }

  *namep = ansp;

  return 1;
}

/* Go through RRs and check or fixup the domain names contained within */
static int check_rrs(unsigned char *p, struct dns_header *header, size_t plen, int fixup, unsigned char **rrs, int rr_count)
{
  int i, j, type, class, rdlen;
  unsigned char *pp;
  
  for (i = 0; i < ntohs(header->ancount) + ntohs(header->nscount) + ntohs(header->arcount); i++)
    {
      pp = p;

      if (!(p = skip_name(p, header, plen, 10)))
	return 0;
      
      GETSHORT(type, p); 
      GETSHORT(class, p);
      p += 4; /* TTL */
      GETSHORT(rdlen, p);

      /* If this RR is to be elided, don't fix up its contents */
      for (j = 0; j < rr_count; j += 2)
	if (rrs[j] == pp)
	  break;

      if (j >= rr_count)
	{
	  /* fixup name of RR */
	  if (!check_name(&pp, header, plen, fixup, rrs, rr_count))
	    return 0;
	  
	  if (class == C_IN)
	    {
	      short *d;
 
	      for (pp = p, d = rrfilter_desc(type); *d != -1; d++)
		{
		  if (*d != 0)
		    pp += *d;
		  else if (!check_name(&pp, header, plen, fixup, rrs, rr_count))
		    return 0;
		}
	    }
	}
      
      if (!ADD_RDLEN(header, p, plen, rdlen))
	return 0;
    }
  
  return 1;
}
	

/* mode may be remove EDNS0 or DNSSEC RRs or remove A or AAAA from answer section.
 * returns number of modified records. */
size_t rrfilter(struct dns_header *header, size_t *plen, int mode)
{
  static unsigned char **rrs = NULL;
  static int rr_sz = 0;

  unsigned char *p = (unsigned char *)(header+1);
  size_t rr_found = 0;
  int i, rdlen, qtype, qclass, chop_an, chop_ns, chop_ar;

  if (mode == RRFILTER_CONF && !daemon->filter_rr)
    return 0;
  
  if (ntohs(header->qdcount) != 1 ||
      !(p = skip_name(p, header, *plen, 4)))
    return 0;
  
  GETSHORT(qtype, p);
  GETSHORT(qclass, p);

  /* First pass, find pointers to start and end of all the records we wish to elide:
     records added for DNSSEC, unless explicitly queried for */
  for (chop_ns = 0, chop_an = 0, chop_ar = 0, i = 0;
       i < ntohs(header->ancount) + ntohs(header->nscount) + ntohs(header->arcount);
       i++)
    {
      unsigned char *pstart = p;
      int type, class;

      if (!(p = skip_name(p, header, *plen, 10)))
	return rr_found;
      
      GETSHORT(type, p); 
      GETSHORT(class, p);
      p += 4; /* TTL */
      GETSHORT(rdlen, p);
        
      if (!ADD_RDLEN(header, p, *plen, rdlen))
	return rr_found;

      if (mode == RRFILTER_EDNS0) /* EDNS */
	{
	  /* EDNS mode, remove T_OPT from additional section only */
	  if (i < (ntohs(header->nscount) + ntohs(header->ancount)) || type != T_OPT)
	    continue;
	}
      else if (mode == RRFILTER_DNSSEC)
	{
	  if (type != T_NSEC && type != T_NSEC3 && type != T_RRSIG)
	    /* DNSSEC mode, remove SIGs and NSECs from all three sections. */
	    continue;

	  /* Don't remove the answer. */
	  if (i < ntohs(header->ancount) && type == qtype && class == qclass)
	    continue;
	}
      else if (qtype == T_ANY && rr_on_list(daemon->filter_rr, T_ANY))
	{
	  /* Filter replies to ANY queries in the spirit of
	     RFC RFC 8482 para 4.3 */
	  if (class != C_IN ||
	      type == T_A || type == T_AAAA || type == T_MX || type == T_CNAME)
	    continue;
	}
      else
	{
	  /* Only looking at answer section now. */
	  if (i >= ntohs(header->ancount))
	    break;

	  if (class != C_IN)
	    continue;
	  
	  if (!rr_on_list(daemon->filter_rr, type))
	    continue;
	}
      
      if (!expand_workspace(&rrs, &rr_sz, rr_found + 1))
	return rr_found;
      
      rrs[rr_found++] = pstart;
      rrs[rr_found++] = p;
      
      if (i < ntohs(header->ancount))
	chop_an++;
      else if (i < (ntohs(header->nscount) + ntohs(header->ancount)))
	chop_ns++;
      else
	chop_ar++;
    }
  
  /* Nothing to do. */
  if (rr_found == 0)
    return rr_found;

  /* Second pass, look for pointers in names in the records we're keeping and make sure they don't
     point to records we're going to elide. This is theoretically possible, but unlikely. If
     it happens, we give up and leave the answer unchanged. */
  p = (unsigned char *)(header+1);
  
  /* question first */
  if (!check_name(&p, header, *plen, 0, rrs, rr_found))
    return rr_found;
  p += 4; /* qclass, qtype */
  
  /* Now answers and NS */
  if (!check_rrs(p, header, *plen, 0, rrs, rr_found))
    return rr_found;
  
  /* Third pass, actually fix up pointers in the records */
  p = (unsigned char *)(header+1);
  
  check_name(&p, header, *plen, 1, rrs, rr_found);
  p += 4; /* qclass, qtype */
  
  check_rrs(p, header, *plen, 1, rrs, rr_found);

  /* Fourth pass, elide records */
  for (p = rrs[0], i = 1; (unsigned)i < rr_found; i += 2)
    {
      unsigned char *start = rrs[i];
      unsigned char *end = ((unsigned)i != rr_found - 1) ? rrs[i+1] : ((unsigned char *)header) + *plen;
      
      memmove(p, start, end-start);
      p += end-start;
    }
     
  *plen = p - (unsigned char *)header;
  header->ancount = htons(ntohs(header->ancount) - chop_an);
  header->nscount = htons(ntohs(header->nscount) - chop_ns);
  header->arcount = htons(ntohs(header->arcount) - chop_ar);

  return rr_found;
}

/* This is used in the DNSSEC code too, hence it's exported */
short *rrfilter_desc(int type)
{
  /* List of RRtypes which include domains in the data.
     0 -> domain
     integer -> no. of plain bytes
     -1 -> end

     zero is not a valid RRtype, so the final entry is returned for
     anything which needs no mangling.
  */
  
  static short rr_desc[] = 
    { 
      T_NS, 0, -1, 
      T_MD, 0, -1,
      T_MF, 0, -1,
      T_CNAME, 0, -1,
      T_SOA, 0, 0, -1,
      T_MB, 0, -1,
      T_MG, 0, -1,
      T_MR, 0, -1,
      T_PTR, 0, -1,
      T_MINFO, 0, 0, -1,
      T_MX, 2, 0, -1,
      T_RP, 0, 0, -1,
      T_AFSDB, 2, 0, -1,
      T_RT, 2, 0, -1,
      T_SIG, 18, 0, -1,
      T_PX, 2, 0, 0, -1,
      T_NXT, 0, -1,
      T_KX, 2, 0, -1,
      T_SRV, 6, 0, -1,
      T_DNAME, 0, -1,
      0, -1 /* wildcard/catchall */
    }; 
  
  short *p = rr_desc;
  
  while (*p != type && *p != 0)
    while (*p++ != -1);

  return p+1;
}

int expand_workspace(unsigned char ***wkspc, int *szp, int new)
{
  unsigned char **p;
  int old = *szp;

  if (old >= new+1)
    return 1;

  new += 5;

  if (!(p = whine_realloc(*wkspc, new * sizeof(unsigned char *))))
    return 0;

  memset(p+old, 0, new-old);
  
  *wkspc = p;
  *szp = new;

  return 1;
}

/* Convert from presentation format to wire format, in place.
   Also map UC -> LC.
   Note that using extract_name to get presentation format
   then calling to_wire() removes compression and maps case,
   thus generating names in canonical form.
   Calling to_wire followed by from_wire is almost an identity,
   except that the UC remains mapped to LC. 

   Note that both /000 and '.' are allowed within labels. These get
   represented in presentation format using NAME_ESCAPE as an escape
   character. In theory, if all the characters in a name were /000 or
   '.' or NAME_ESCAPE then all would have to be escaped, so the 
   presentation format would be twice as long as the spec (1024). 
   The buffers are all declared as 2049 (allowing for the trailing zero) 
   for this reason.
*/
int to_wire(char *name)
{
  unsigned char *l, *p, *q, term;
  int len;

  for (l = (unsigned char*)name; *l != 0; l = p)
    {
      for (p = l; *p != '.' && *p != 0; p++)
	if (*p >= 'A' && *p <= 'Z')
	  *p = *p - 'A' + 'a';
	else if (*p == NAME_ESCAPE)
	  {
	    for (q = p; *q; q++)
	      *q = *(q+1);
	    (*p)--;
	  }
      term = *p;
      
      if ((len = p - l) != 0)
	memmove(l+1, l, len);
      *l = len;
      
      p++;
      
      if (term == 0)
	*p = 0;
    }
  
  return l + 1 - (unsigned char *)name;
}

/* Note: no compression  allowed in input. */
void from_wire(char *name)
{
  unsigned char *l, *p, *last;
  int len;
  
  for (last = (unsigned char *)name; *last != 0; last += *last+1);
  
  for (l = (unsigned char *)name; *l != 0; l += len+1)
    {
      len = *l;
      memmove(l, l+1, len);
      for (p = l; p < l + len; p++)
	if (*p == '.' || *p == 0 || *p == NAME_ESCAPE)
	  {
	    memmove(p+1, p, 1 + last - p);
	    len++;
	    *p++ = NAME_ESCAPE; 
	    (*p)++;
	  }
	
      l[len] = '.';
    }

  if ((char *)l != name)
    *(l-1) = 0;
}
