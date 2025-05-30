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

#include "dnsmasq.h"


static struct cond_domain *search_domain(struct in_addr addr, struct cond_domain *c);
static int match_domain(struct in_addr addr, struct cond_domain *c);
#ifdef HAVE_IPV6
static struct cond_domain *search_domain6(struct in6_addr *addr, struct cond_domain *c);
static int match_domain6(struct in6_addr *addr, struct cond_domain *c);
#endif /* HAVE_IPV6 */

int is_name_synthetic(int flags, char *name, union all_addr *addrp)
{
  char *p;
  struct cond_domain *c = NULL;
  int prot = AF_INET;

#ifdef HAVE_IPV6
  if (flags & F_IPV6)
    prot = AF_INET6;
#endif /* HAVE_IPV6 */
  union all_addr addr;
  
  for (c = daemon->synth_domains; c; c = c->next)
    {
      int found = 0;
      char *tail, *pref;
      
      for (tail = name, pref = c->prefix; *tail != 0 && pref && *pref != 0; tail++, pref++)
	{
	  unsigned int c1 = (unsigned char) *pref;
	  unsigned int c2 = (unsigned char) *tail;
	  
	  if (c1 >= 'A' && c1 <= 'Z')
	    c1 += 'a' - 'A';
	  if (c2 >= 'A' && c2 <= 'Z')
	    c2 += 'a' - 'A';
	  
	  if (c1 != c2)
	    break;
	}
      
      if (pref && *pref != 0)
	continue; /* prefix match fail */

      if (c->indexed)
	{
	  for (p = tail; *p; p++)
	    {
	      char c = *p;
	      
	      if (c < '0' || c > '9')
		break;
	    }
	  
	  if (*p != '.')
	    continue;
	  
	  *p = 0;
	  
	  if (hostname_isequal(c->domain, p+1))
	    {
	      if (prot == AF_INET)
		{
		  unsigned int index = atoi(tail);

		   if (!c->is6 &&
		      index <= ntohl(c->end.s_addr) - ntohl(c->start.s_addr))
		    {
		      addr.addr4.s_addr = htonl(ntohl(c->start.s_addr) + index);
		      found = 1;
		    }
		} 
#ifdef HAVE_IPV6 
	      else
		{
		  u64 index = atoll(tail);
		  
		  if (c->is6 &&
		      index <= addr6part(&c->end6) - addr6part(&c->start6))
		    {
		      u64 start = addr6part(&c->start6);
		      addr.addr6 = c->start6;
		      setaddr6part(&addr.addr6, start + index);
		      found = 1;
		    }
		}
#endif /* HAVE_IPV6 */
	    }
	}
      else
	{
	  /* NB, must not alter name if we return zero */
	  for (p = tail; *p; p++)
	    {
	      char c = *p;
	      
	      if ((c >='0' && c <= '9') || c == '-')
		continue;
	      
#ifdef HAVE_IPV6
	      if (prot == AF_INET6 && ((c >='A' && c <= 'F') || (c >='a' && c <= 'f'))) 
		continue;
#endif /* HAVE_IPV6 */
	      
	      break;
	    }
	  
	  if (*p != '.')
	    continue;
	  
	  *p = 0;	
	  
	  /* swap . or : for - */
	  for (p = tail; *p; p++)
	    if (*p == '-')
	      {
		if (prot == AF_INET)
		  *p = '.';
#ifdef HAVE_IPV6
		else
		  *p = ':';
#endif /* HAVE_IPV6 */
	      }
	  
	  if (hostname_isequal(c->domain, p+1) && inet_pton(prot, tail, &addr)) {
	    if (prot == AF_INET)
		  found = match_domain(addr.addr4, c);
#ifdef HAVE_IPV6
	    else
		  found = match_domain6(&addr.addr6, c);
#endif /* HAVE_IPV6 */
	  }
	}
      
      /* restore name */
      for (p = tail; *p; p++)
	if (*p == '.' || *p == ':')
	  *p = '-';
      
      *p = '.';
      
      
      if (found)
	{
	  if (addrp)
	    *addrp = addr;
	  
	  return 1;
	}
    }
  
  return 0;
}


int is_rev_synth(int flag, union all_addr *addr, char *name)
{
   struct cond_domain *c;

   if (flag & F_IPV4 && (c = search_domain(addr->addr4, daemon->synth_domains))) 
     {
       char *p;
       
       *name = 0;
       if (c->indexed)
	 {
	   unsigned int index = ntohl(addr->addr4.s_addr) - ntohl(c->start.s_addr);
	   snprintf(name, MAXDNAME, "%s%u", c->prefix ? c->prefix : "", index);
	 }
       else
	 {
	   if (c->prefix)
	     strncpy(name, c->prefix, MAXDNAME - ADDRSTRLEN);
       
       	   inet_ntop(AF_INET, &addr->addr4, name + strlen(name), ADDRSTRLEN);
	   for (p = name; *p; p++)
	     if (*p == '.')
	       *p = '-';
	 }
       
       strncat(name, ".", MAXDNAME);
       strncat(name, c->domain, MAXDNAME);

       return 1;
     }

#ifdef HAVE_IPV6
   if ((flag & F_IPV6) && (c = search_domain6(&addr->addr6, daemon->synth_domains))) 
     {
       *name = 0;

       if (c->indexed)
	 {
	   u64 index = addr6part(&addr->addr6) - addr6part(&c->start6);
	   snprintf(name, MAXDNAME, "%s%llu", c->prefix ? c->prefix : "", index);
	 }
       else
	 {
	   int i;
	   char frag[6];

	   if (c->prefix)
	     strncpy(name, c->prefix, MAXDNAME);
	   
	   for (i = 0; i < 16; i += 2)
	     {
	       sprintf(frag, "%s%02x%02x",  i == 0 ? "" : "-", addr->addr6.s6_addr[i], addr->addr6.s6_addr[i+1]);
	       strncat(name, frag, MAXDNAME);
	     }
	 }

       strncat(name, ".", MAXDNAME);
       strncat(name, c->domain, MAXDNAME);
       
       return 1;
     }
#endif /* HAVE_IPV6 */
   
   return 0;
}


static int match_domain(struct in_addr addr, struct cond_domain *c)
{
  if (c->interface)
    {
      struct addrlist *al;
      for (al = c->al; al; al = al->next)
	if (!(al->flags & ADDRLIST_IPV6) &&
	    is_same_net_prefix(addr, al->addr.addr4, al->prefixlen))
	  return 1;
    }
  else if (!c->is6 &&
	   ntohl(addr.s_addr) >= ntohl(c->start.s_addr) &&
	   ntohl(addr.s_addr) <= ntohl(c->end.s_addr))
    return 1;

  return 0;
}

static struct cond_domain *search_domain(struct in_addr addr, struct cond_domain *c)
{
  for (; c; c = c->next)
    if (match_domain(addr, c))
      return c;
  
  return NULL;
}

char *get_domain(struct in_addr addr)
{
  struct cond_domain *c;

  if ((c = search_domain(addr, daemon->cond_domain)))
    return c->domain;

  return daemon->domain_suffix;
} 

#ifdef HAVE_IPV6
static int match_domain6(struct in6_addr *addr, struct cond_domain *c)
{
    
  /* subnet from interface address. */
  if (c->interface)
    {
      struct addrlist *al;
      for (al = c->al; al; al = al->next)
	if (al->flags & ADDRLIST_IPV6 &&
	    is_same_net6(addr, &al->addr.addr6, al->prefixlen))
	  return 1;
    }
  else if (c->is6)
    {
      if (c->prefixlen >= 64)
	{
	  u64 addrpart = addr6part(addr);
	  if (is_same_net6(addr, &c->start6, 64) &&
	      addrpart >= addr6part(&c->start6) &&
	      addrpart <= addr6part(&c->end6))
	    return 1;
	}
      else if (is_same_net6(addr, &c->start6, c->prefixlen))
	return 1;
    }
    
  return 0;
}

static struct cond_domain *search_domain6(struct in6_addr *addr, struct cond_domain *c)
{
  for (; c; c = c->next)
    if (match_domain6(addr, c))
      return c;
  
  return NULL;
}

char *get_domain6(struct in6_addr *addr)
{
  struct cond_domain *c;

  if (addr && (c = search_domain6(addr, daemon->cond_domain)))
    return c->domain;

  return daemon->domain_suffix;
} 
#endif /* HAVE_IPV6 */
