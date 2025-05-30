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

#ifdef HAVE_AUTH

static struct addrlist *find_addrlist(struct addrlist *list, int flag, union all_addr *addr_u)
{
  do {
    if (!(list->flags & ADDRLIST_IPV6))
      {
	struct in_addr netmask, addr = addr_u->addr4;
	
	if (!(flag & F_IPV4))
	  continue;
	
	netmask.s_addr = htonl(~(in_addr_t)0 << (32 - list->prefixlen));
	
	if  (is_same_net(addr, list->addr.addr4, netmask))
	  return list;
      }
#ifdef HAVE_IPV6
    else if (is_same_net6(&(addr_u->addr6), &list->addr.addr6, list->prefixlen))
      return list;
#endif /* HAVE_IPV6 */
    
  } while ((list = list->next));
  
  return NULL;
}

static struct addrlist *find_subnet(struct auth_zone *zone, int flag, union all_addr *addr_u)
{
  if (!zone->subnet)
    return NULL;
  
  return find_addrlist(zone->subnet, flag, addr_u);
}

static struct addrlist *find_exclude(struct auth_zone *zone, int flag, union all_addr *addr_u)
{
  if (!zone->exclude)
    return NULL;
  
  return find_addrlist(zone->exclude, flag, addr_u);
}

static int filter_zone(struct auth_zone *zone, int flag, union all_addr *addr_u)
{
  if (find_exclude(zone, flag, addr_u))
    return 0;

  /* No subnets specified, no filter */
  if (!zone->subnet)
    return 1;
  
  return find_subnet(zone, flag, addr_u) != NULL;
}

int in_zone(struct auth_zone *zone, char *name, char **cut)
{
  size_t namelen = strlen(name);
  size_t domainlen = strlen(zone->domain);

  if (cut)
    *cut = NULL;
  
  if (namelen >= domainlen && 
      hostname_isequal(zone->domain, &name[namelen - domainlen]))
    {
      
      if (namelen == domainlen)
	return 1;
      
      if (name[namelen - domainlen - 1] == '.')
	{
	  if (cut)
	    *cut = &name[namelen - domainlen - 1]; 
	  return 1;
	}
    }

  return 0;
}


size_t answer_auth(struct dns_header *header, char *limit, size_t qlen, time_t now,
		   union mysockaddr *peer_addr, int local_query) 
{
  char *name = daemon->namebuff;
  unsigned char *p, *ansp;
  int qtype, qclass, rc;
  int nameoffset, axfroffset = 0;
  int q, anscount = 0, authcount = 0;
  struct crec *crecp;
  int  auth = !local_query, trunc = 0, nxdomain = 1, soa = 0, ns = 0, axfr = 0, out_of_zone = 0;
  struct auth_zone *zone = NULL;
  struct addrlist *subnet = NULL;
  char *cut;
  struct mx_srv_record *rec, *move, **up;
  struct txt_record *txt;
  struct interface_name *intr;
  struct naptr *na;
  union all_addr addr;
  struct cname *a, *candidate;
  unsigned int wclen;
  
  if (ntohs(header->qdcount) == 0 || OPCODE(header) != QUERY )
    return 0;

  /* determine end of question section (we put answers there) */
  if (!(ansp = skip_questions(header, qlen)))
    return 0; /* bad packet */
  
  /* now process each question, answers go in RRs after the question */
  p = (unsigned char *)(header+1);

  for (q = ntohs(header->qdcount); q != 0; q--)
    {
      unsigned int flag = 0;
      int found = 0;
      int cname_wildcard = 0;
  
      /* save pointer to name for copying into answers */
      nameoffset = p - (unsigned char *)header;

      /* now extract name as .-concatenated string into name */
      if (!extract_name(header, qlen, &p, name, EXTR_NAME_EXTRACT, 4))
	return 0; /* bad packet */
 
      GETSHORT(qtype, p); 
      GETSHORT(qclass, p);
      
      if (qclass != C_IN)
	{
	  auth = 0;
	  out_of_zone = 1;
	  continue;
	}

      if ((qtype == T_PTR || qtype == T_SOA || qtype == T_NS) &&
	  (flag = in_arpa_name_2_addr(name, &addr)) &&
	  !local_query)
	{
	  for (zone = daemon->auth_zones; zone; zone = zone->next)
	    if ((subnet = find_subnet(zone, flag, &addr)))
	      break;
	  
	  if (!zone)
	    {
	      out_of_zone = 1;
	      auth = 0;
	      continue;
	    }
	  else if (qtype == T_SOA)
	    soa = 1, found = 1;
	  else if (qtype == T_NS)
	    ns = 1, found = 1;
	}

      if (qtype == T_PTR && flag)
	{
	  intr = NULL;

	  if (flag == F_IPV4)
	    for (intr = daemon->int_names; intr; intr = intr->next)
	      {
		struct addrlist *addrlist;
		
		for (addrlist = intr->addr; addrlist; addrlist = addrlist->next)
		  if (!(addrlist->flags & ADDRLIST_IPV6) && addr.addr4.s_addr == addrlist->addr.addr4.s_addr)
		    break;
		
		if (addrlist)
		  break;
		else
		  while (intr->next && strcmp(intr->intr, intr->next->intr) == 0)
		    intr = intr->next;
	      }
#ifdef HAVE_IPV6
	  else if (flag == F_IPV6)
	    for (intr = daemon->int_names; intr; intr = intr->next)
	      {
		struct addrlist *addrlist;
		
		for (addrlist = intr->addr; addrlist; addrlist = addrlist->next)
		  if ((addrlist->flags & ADDRLIST_IPV6) && IN6_ARE_ADDR_EQUAL(&addr.addr6, &addrlist->addr.addr6))
		    break;
		
		if (addrlist)
		  break;
		else
		  while (intr->next && strcmp(intr->intr, intr->next->intr) == 0)
		    intr = intr->next;
	      }
#endif /* HAVE_IPV6 */
	  
	  if (intr)
	    {
	      if (local_query || in_zone(zone, intr->name, NULL))
		{	
		  found = 1;
		  log_query(flag | F_REVERSE | F_CONFIG, intr->name, &addr, NULL, 0);
		  if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					  daemon->auth_ttl, NULL,
					  T_PTR, C_IN, "d", intr->name))
		    anscount++;
		}
	    }
	  
	  if ((crecp = cache_find_by_addr(NULL, &addr, now, flag)))
	    do { 
	      strcpy(name, cache_get_name(crecp));
	      
	      if (crecp->flags & F_DHCP && !option_bool(OPT_DHCP_FQDN))
		{
		  char *p = strchr(name, '.');
		  if (p)
		    *p = 0; /* must be bare name */
		  
		  /* add  external domain */
		  if (zone)
		    {
		      strcat(name, ".");
		      strcat(name, zone->domain);
		    }
		  log_query(flag | F_DHCP | F_REVERSE, name, &addr, record_source(crecp->uid), 0);
		  found = 1;
		  if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					  daemon->auth_ttl, NULL,
					  T_PTR, C_IN, "d", name))
		    anscount++;
		}
	      else if (crecp->flags & (F_DHCP | F_HOSTS) && (local_query || in_zone(zone, name, NULL)))
		{
		  log_query(crecp->flags & ~F_FORWARD, name, &addr, record_source(crecp->uid), 0);
		  found = 1;
		  if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					  daemon->auth_ttl, NULL,
					  T_PTR, C_IN, "d", name))
		    anscount++;
		}
	      else
		continue;
		    
	    } while ((crecp = cache_find_by_addr(crecp, &addr, now, flag)));

	  if (!found && is_rev_synth(flag, &addr, name) && (local_query || in_zone(zone, name, NULL)))
	    {
	      log_query(F_CONFIG | F_REVERSE | flag, name, &addr, NULL, 0);
	      found = 1;
	      
	      if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
				      daemon->auth_ttl, NULL,
				      T_PTR, C_IN, "d", name))
		anscount++;
	    }

	  if (found)
	    nxdomain = 0;
	  else
	    log_query(flag | F_NEG | F_NXDOMAIN | F_REVERSE | (auth ? F_AUTH : 0), NULL, &addr, NULL, 0);

	  continue;
	}
      
    cname_restart:
      if (found)
	/* NS and SOA .arpa requests have set found above. */
	cut = NULL;
      else
	{
	  for (zone = daemon->auth_zones; zone; zone = zone->next)
	    if (in_zone(zone, name, &cut))
	      break;
	  
	  if (!zone)
	    {
	      out_of_zone = 1;
	      auth = 0;
	      continue;
	    }
	}

      for (rec = daemon->mxnames; rec; rec = rec->next)
	if (!rec->issrv && (rc = hostname_issubdomain(name, rec->name)))
	  {
	    nxdomain = 0;
	         
	    if (rc == 2 && qtype == T_MX)
	      {
		found = 1;
		log_query(F_CONFIG | F_RRNAME, name, NULL, "<MX>", 0);
		if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, daemon->auth_ttl,
					NULL, T_MX, C_IN, "sd", rec->weight, rec->target))
		  anscount++;
	      }
	  }
      
      for (move = NULL, up = &daemon->mxnames, rec = daemon->mxnames; rec; rec = rec->next)
	if (rec->issrv && (rc = hostname_issubdomain(name, rec->name)))
	  {
	    nxdomain = 0;
	    
	    if (rc == 2 && qtype == T_SRV)
	      {
		found = 1;
		log_query(F_CONFIG | F_RRNAME, name, NULL, "<SRV>", 0);
		if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, daemon->auth_ttl,
					NULL, T_SRV, C_IN, "sssd", 
					rec->priority, rec->weight, rec->srvport, rec->target))

		  anscount++;
	      } 
	    
	    /* unlink first SRV record found */
	    if (!move)
	      {
		move = rec;
		*up = rec->next;
	      }
	    else
	      up = &rec->next;      
	  }
	else
	  up = &rec->next;
	  
      /* put first SRV record back at the end. */
      if (move)
	{
	  *up = move;
	  move->next = NULL;
	}

      for (txt = daemon->rr; txt; txt = txt->next)
	if ((rc = hostname_issubdomain(name, txt->name)))
	  {
	    nxdomain = 0;
	    if (rc == 2 && txt->class == qtype)
	      {
		found = 1;
		log_query(F_CONFIG | F_RRNAME, name, NULL, NULL, txt->class);
		if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, daemon->auth_ttl,
					NULL, txt->class, C_IN, "t", txt->len, txt->txt))
		  anscount++;
	      }
	  }
      
      for (txt = daemon->txt; txt; txt = txt->next)
	if (txt->class == C_IN && (rc = hostname_issubdomain(name, txt->name)))
	  {
	    nxdomain = 0;
	    if (rc == 2 && qtype == T_TXT)
	      {
		found = 1;
		log_query(F_CONFIG | F_RRNAME, name, NULL, "<TXT>", 0);
		if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, daemon->auth_ttl,
					NULL, T_TXT, C_IN, "t", txt->len, txt->txt))
		  anscount++;
	      }
	  }

       for (na = daemon->naptr; na; na = na->next)
	 if ((rc = hostname_issubdomain(name, na->name)))
	   {
	     nxdomain = 0;
	     if (rc == 2 && qtype == T_NAPTR)
	       {
		 found = 1;
		 log_query(F_CONFIG | F_RRNAME, name, NULL, "<NAPTR>", 0);
		 if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, daemon->auth_ttl, 
					 NULL, T_NAPTR, C_IN, "sszzzd", 
					 na->order, na->pref, na->flags, na->services, na->regexp, na->replace))
			  anscount++;
	       }
	   }
    
       if (qtype == T_A)
	 flag = F_IPV4;
       
#ifdef HAVE_IPV6
       if (qtype == T_AAAA)
	 flag = F_IPV6;
#endif /* HAVE_IPV6 */
       
       for (intr = daemon->int_names; intr; intr = intr->next)
	 if ((rc = hostname_issubdomain(name, intr->name)))
	   {
	     struct addrlist *addrlist;
	     
	     nxdomain = 0;
	     
	     if (rc == 2 && flag)
	       for (addrlist = intr->addr; addrlist; addrlist = addrlist->next)  
		 if (((addrlist->flags & ADDRLIST_IPV6)  ? T_AAAA : T_A) == qtype &&
		     (local_query || filter_zone(zone, flag, &addrlist->addr)))
		   {
#ifdef HAVE_IPV6
		     if (addrlist->flags & ADDRLIST_REVONLY)
		       continue;

#endif /* HAVE_IPV6 */
		     found = 1;
		     log_query(F_FORWARD | F_CONFIG | flag, name, &addrlist->addr, NULL, 0);
		     if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					     daemon->auth_ttl, NULL, qtype, C_IN, 
					     qtype == T_A ? "4" : "6", &addrlist->addr))
		       anscount++;
		   }
	     }

       if (!found && is_name_synthetic(flag, name, &addr) )
	 {
	   nxdomain = 0;
	   
	   log_query(F_FORWARD | F_CONFIG | flag, name, &addr, NULL, 0);
	   if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
				   daemon->auth_ttl, NULL, qtype, C_IN, qtype == T_A ? "4" : "6", &addr))
	     anscount++;
	 }
       
      if (!cut)
	{
	  nxdomain = 0;
	  
	  if (qtype == T_SOA)
	    {
	      auth = soa = 1; /* inhibits auth section */
	      log_query(F_RRNAME | F_AUTH, zone->domain, NULL, "<SOA>", 0);
	    }
      	  else if (qtype == T_AXFR)
	    {
	      struct iname *peers;
	      
	      if (peer_addr->sa.sa_family == AF_INET)
		peer_addr->in.sin_port = 0;
#ifdef HAVE_IPV6
	      else
		{
		  peer_addr->in6.sin6_port = 0; 
		  peer_addr->in6.sin6_scope_id = 0;
		}
#endif /* HAVE_IPV6 */
	      
	      for (peers = daemon->auth_peers; peers; peers = peers->next)
		if (sockaddr_isequal(peer_addr, &peers->addr))
		  break;
	      
	      /* Refuse all AXFR unless --auth-sec-servers or auth-peers is set */
	      if ((!daemon->secondary_forward_server && !daemon->auth_peers) ||
		  (daemon->auth_peers && !peers)) 
		{
		  if (peer_addr->sa.sa_family == AF_INET)
		    inet_ntop(AF_INET, &peer_addr->in.sin_addr, daemon->addrbuff, ADDRSTRLEN);
#ifdef HAVE_IPV6
		  else
		    inet_ntop(AF_INET6, &peer_addr->in6.sin6_addr, daemon->addrbuff, ADDRSTRLEN); 
#endif /* HAVE_IPV6 */
		  
		  my_syslog(LOG_WARNING, _("ignoring zone transfer request from %s"), daemon->addrbuff);
		  return 0;
		}
	       	      
	      auth = 1;
	      soa = 1; /* inhibits auth section */
	      ns = 1; /* ensure we include NS records! */
	      axfr = 1;
	      axfroffset = nameoffset;
	      log_query(F_RRNAME | F_AUTH, zone->domain, NULL, "<AXFR>", 0);
	    }
      	  else if (qtype == T_NS)
	    {
	      auth = 1;
	      ns = 1; /* inhibits auth section */
	      log_query(F_RRNAME | F_AUTH, zone->domain, NULL, "<NS>", 0);
	    }
	}
      
      if (!option_bool(OPT_DHCP_FQDN) && cut)
	{	  
	  *cut = 0; /* remove domain part */
	  
	  if (!strchr(name, '.') && (crecp = cache_find_by_name(NULL, name, now, F_IPV4 | F_IPV6)))
	    {
	      if (crecp->flags & F_DHCP)
		do
		  { 
		    nxdomain = 0;
		    if ((crecp->flags & flag) && 
			(local_query || filter_zone(zone, flag, &(crecp->addr))))
		      {
			*cut = '.'; /* restore domain part */
			log_query(crecp->flags, name, &crecp->addr, record_source(crecp->uid), 0);
			*cut  = 0; /* remove domain part */
			if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
						daemon->auth_ttl, NULL, qtype, C_IN, 
						qtype == T_A ? "4" : "6", &crecp->addr))
			  anscount++;
		      }
		  } while ((crecp = cache_find_by_name(crecp, name, now,  F_IPV4 | F_IPV6)));
	    }
       	  
	  *cut = '.'; /* restore domain part */	    
	}
      
      if ((crecp = cache_find_by_name(NULL, name, now, F_IPV4 | F_IPV6)))
	{
	  if ((crecp->flags & F_HOSTS) || (((crecp->flags & F_DHCP) && option_bool(OPT_DHCP_FQDN))))
	    do
	      { 
		 nxdomain = 0;
		 if ((crecp->flags & flag) && (local_query || filter_zone(zone, flag, &(crecp->addr))))
		   {
		     log_query(crecp->flags & ~F_REVERSE, name, &crecp->addr, record_source(crecp->uid), 0);
		     if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
					     daemon->auth_ttl, NULL, qtype, C_IN, 
					     qtype == T_A ? "4" : "6", &crecp->addr))
		       anscount++;
		   }
	      } while ((crecp = cache_find_by_name(crecp, name, now, F_IPV4 | F_IPV6)));
	}
      
      /* Only supply CNAME if no record for any type is known. */
      if (nxdomain)
	{
	  /* Check for possible wildcard match against *.domain 
	     return length of match, to get longest.
	     Note that if return length of wildcard section, so
	     we match b.simon to _both_ *.simon and b.simon
	     but return a longer (better) match to b.simon.
	  */  
	  for (wclen = 0, candidate = NULL, a = daemon->cnames; a; a = a->next)
	    if (a->alias[0] == '*')
	      {
		char *test = name;
		
		while ((test = strchr(test+1, '.')))
		  {
		    if (hostname_isequal(test, &(a->alias[1])))
		      {
			if (strlen(test) > wclen && !cname_wildcard)
			  {
			    wclen = strlen(test);
			    candidate = a;
			    cname_wildcard = 1;
			  }
			break;
		      }
		  }
		
	      }
	    else if (hostname_isequal(a->alias, name) && strlen(a->alias) > wclen)
	      {
		/* Simple case, no wildcard */
		wclen = strlen(a->alias);
		candidate = a;
	      }
	  
	  if (candidate)
	    {
	      log_query(F_CONFIG | F_CNAME, name, NULL, NULL, 0);
	      strcpy(name, candidate->target);
	      if (!strchr(name, '.'))
		{
		  strcat(name, ".");
		  strcat(name, zone->domain);
		}
	      found = 1;
	      if (add_resource_record(header, limit, &trunc, nameoffset, &ansp, 
				      daemon->auth_ttl, &nameoffset,
				      T_CNAME, C_IN, "d", name))
		anscount++;
	      
	      goto cname_restart;
	    }
	  else if (cache_find_non_terminal(name, now))
	    nxdomain = 0;

	  log_query(flag | F_NEG | (nxdomain ? F_NXDOMAIN : 0) | F_FORWARD | F_AUTH, name, NULL, NULL, 0);
	}
      
    }
  
  /* Add auth section */
  if (auth && zone)
    {
      char *authname;
      int newoffset, offset = 0;

      if (!subnet)
	authname = zone->domain;
      else
	{
	  /* handle NS and SOA for PTR records */
	  
	  authname = name;

	  if (!(subnet->flags & ADDRLIST_IPV6))
	    {
	      in_addr_t a = ntohl(subnet->addr.addr4.s_addr) >> 8;
	      char *p = name;
	      
	      if (subnet->prefixlen >= 24)
		p += sprintf(p, "%u.", a & 0xff);
	      a = a >> 8;
	      if (subnet->prefixlen >= 16 )
		p += sprintf(p, "%u.", a & 0xff);
	      a = a >> 8;
	      sprintf(p, "%u.in-addr.arpa", a & 0xff);
	      
	    }
#ifdef HAVE_IPV6
	  else
	    {
	      char *p = name;
	      int i;
	      
	      for (i = subnet->prefixlen-1; i >= 0; i -= 4)
		{ 
		  int dig = ((unsigned char *)&subnet->addr.addr6)[i>>3];
		  p += sprintf(p, "%.1x.", (i>>2) & 1 ? dig & 15 : dig >> 4);
		}
	      sprintf(p, "ip6.arpa");
	      
	    }
#endif /* HAVE_IPV6 */
	}
      
      /* handle NS and SOA in auth section or for explicit queries */
       newoffset = ansp - (unsigned char *)header;
       if (((anscount == 0 && !ns) || soa) &&
	  add_resource_record(header, limit, &trunc, 0, &ansp, 
			      daemon->auth_ttl, NULL, T_SOA, C_IN, "ddlllll",
			      authname, daemon->authserver,  daemon->hostmaster,
			      daemon->soa_sn, daemon->soa_refresh, 
			      daemon->soa_retry, daemon->soa_expiry, 
			      daemon->auth_ttl))
	{
	  offset = newoffset;
	  if (soa)
	    anscount++;
	  else
	    authcount++;
	}
      
      if (anscount != 0 || ns)
	{
	  struct name_list *secondary;
	  
	  /* Only include the machine running dnsmasq if it's acting as an auth server */
	  if (daemon->authinterface)
	    {
	      newoffset = ansp - (unsigned char *)header;
	      if (add_resource_record(header, limit, &trunc, -offset, &ansp, 
				      daemon->auth_ttl, NULL, T_NS, C_IN, "d", offset == 0 ? authname : NULL, daemon->authserver))
		{
		  if (offset == 0) 
		    offset = newoffset;
		  if (ns) 
		    anscount++;
		  else
		    authcount++;
		}
	    }

	  if (!subnet)
	    for (secondary = daemon->secondary_forward_server; secondary; secondary = secondary->next)
	      if (add_resource_record(header, limit, &trunc, offset, &ansp, 
				      daemon->auth_ttl, NULL, T_NS, C_IN, "d", secondary->name))
		{
		  if (ns) 
		    anscount++;
		  else
		    authcount++;
		}
	}
      
      if (axfr)
	{
	  for (rec = daemon->mxnames; rec; rec = rec->next)
	    if (in_zone(zone, rec->name, &cut))
	      {
		if (cut)
		   *cut = 0;

		if (rec->issrv)
		  {
		    if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, daemon->auth_ttl,
					    NULL, T_SRV, C_IN, "sssd", cut ? rec->name : NULL,
					    rec->priority, rec->weight, rec->srvport, rec->target))
		      
		      anscount++;
		  }
		else
		  {
		    if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, daemon->auth_ttl,
					    NULL, T_MX, C_IN, "sd", cut ? rec->name : NULL, rec->weight, rec->target))
		      anscount++;
		  }
		
		/* restore config data */
		if (cut)
		  *cut = '.';
	      }
	      
	  for (txt = daemon->rr; txt; txt = txt->next)
	    if (in_zone(zone, txt->name, &cut))
	      {
		if (cut)
		  *cut = 0;
		
		if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, daemon->auth_ttl,
					NULL, txt->class, C_IN, "t",  cut ? txt->name : NULL, txt->len, txt->txt))
		  anscount++;
		
		/* restore config data */
		if (cut)
		  *cut = '.';
	      }
	  
	  for (txt = daemon->txt; txt; txt = txt->next)
	    if (txt->class == C_IN && in_zone(zone, txt->name, &cut))
	      {
		if (cut)
		  *cut = 0;
		
		if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, daemon->auth_ttl,
					NULL, T_TXT, C_IN, "t", cut ? txt->name : NULL, txt->len, txt->txt))
		  anscount++;
		
		/* restore config data */
		if (cut)
		  *cut = '.';
	      }
	  
	  for (na = daemon->naptr; na; na = na->next)
	    if (in_zone(zone, na->name, &cut))
	      {
		if (cut)
		  *cut = 0;
		
		if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, daemon->auth_ttl, 
					NULL, T_NAPTR, C_IN, "sszzzd", cut ? na->name : NULL,
					na->order, na->pref, na->flags, na->services, na->regexp, na->replace))
		  anscount++;
		
		/* restore config data */
		if (cut)
		  *cut = '.'; 
	      }
	  
	  for (intr = daemon->int_names; intr; intr = intr->next)
	    if (in_zone(zone, intr->name, &cut))
	      {
		struct addrlist *addrlist;
		
		if (cut)
		  *cut = 0;
		
		for (addrlist = intr->addr; addrlist; addrlist = addrlist->next) 
		  if (!(addrlist->flags & ADDRLIST_IPV6) &&
		      (local_query || filter_zone(zone, F_IPV4, &addrlist->addr)) && 
		      add_resource_record(header, limit, &trunc, -axfroffset, &ansp, 
					  daemon->auth_ttl, NULL, T_A, C_IN, "4", cut ? intr->name : NULL, &addrlist->addr))
		    anscount++;
		
#ifdef HAVE_IPV6
		for (addrlist = intr->addr; addrlist; addrlist = addrlist->next) 
		  if ((addrlist->flags & ADDRLIST_IPV6) && 
		      (local_query || filter_zone(zone, F_IPV6, &addrlist->addr)) &&
		      add_resource_record(header, limit, &trunc, -axfroffset, &ansp, 
					  daemon->auth_ttl, NULL, T_AAAA, C_IN, "6", cut ? intr->name : NULL, &addrlist->addr))
		    anscount++;
#endif /* HAVE_IPV6 */
		
		/* restore config data */
		if (cut)
		  *cut = '.'; 
	      }
             
	  for (a = daemon->cnames; a; a = a->next)
	    if (in_zone(zone, a->alias, &cut))
	      {
		strcpy(name, a->target);
		if (!strchr(name, '.'))
		  {
		    strcat(name, ".");
		    strcat(name, zone->domain);
		  }
		
		if (cut)
		  *cut = 0;
		
		if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, 
					daemon->auth_ttl, NULL,
					T_CNAME, C_IN, "d",  cut ? a->alias : NULL, name))
		  anscount++;
	      }
	
	  cache_enumerate(1);
	  while ((crecp = cache_enumerate(0)))
	    {
	      if ((crecp->flags & (F_IPV4 | F_IPV6)) &&
		  !(crecp->flags & (F_NEG | F_NXDOMAIN)) &&
		  (crecp->flags & F_FORWARD))
		{
		  if ((crecp->flags & F_DHCP) && !option_bool(OPT_DHCP_FQDN))
		    {
		      char *cache_name = cache_get_name(crecp);
		      if (!strchr(cache_name, '.') && 
			  (local_query || filter_zone(zone, (crecp->flags & (F_IPV6 | F_IPV4)), &(crecp->addr))))
			{
			  qtype = T_A;
#ifdef HAVE_IPV6
			  if (crecp->flags & F_IPV6)
			    qtype = T_AAAA;
#endif /* HAVE_IPV6 */
			  if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, 
						  daemon->auth_ttl, NULL, qtype, C_IN, 
						  (crecp->flags & F_IPV4) ? "4" : "6", cache_name, &crecp->addr))
			    anscount++;
			}
		    }
		  
		  if ((crecp->flags & F_HOSTS) || (((crecp->flags & F_DHCP) && option_bool(OPT_DHCP_FQDN))))
		    {
		      strcpy(name, cache_get_name(crecp));
		      if (in_zone(zone, name, &cut) && 
			  (local_query || filter_zone(zone, (crecp->flags & (F_IPV6 | F_IPV4)), &(crecp->addr))))
			{
			  qtype = T_A;
#ifdef HAVE_IPV6
			  if (crecp->flags & F_IPV6)
			    qtype = T_AAAA;
#endif /* HAVE_IPV6 */
			   if (cut)
			     *cut = 0;

			   if (add_resource_record(header, limit, &trunc, -axfroffset, &ansp, 
						   daemon->auth_ttl, NULL, qtype, C_IN, 
						   (crecp->flags & F_IPV4) ? "4" : "6", cut ? name : NULL, &crecp->addr))
			     anscount++;
			}
		    }
		}
	    }
	   
	  /* repeat SOA as last record */
	  if (add_resource_record(header, limit, &trunc, axfroffset, &ansp, 
				  daemon->auth_ttl, NULL, T_SOA, C_IN, "ddlllll",
				  daemon->authserver,  daemon->hostmaster,
				  daemon->soa_sn, daemon->soa_refresh, 
				  daemon->soa_retry, daemon->soa_expiry, 
				  daemon->auth_ttl))
	    anscount++;
	  
	}
      
    }
  
  /* done all questions, set up header and return length of result */
  /* clear authoritative and truncated flags, set QR flag */
  header->hb3 = (header->hb3 & ~(HB3_AA | HB3_TC)) | HB3_QR;

  if (local_query)
    {
      /* set RA flag */
      header->hb4 |= HB4_RA;
    }
  else
    {
      /* clear RA flag */
      header->hb4 &= ~HB4_RA;
    }

  /* data is never DNSSEC signed. */
  header->hb4 &= ~HB4_AD;

  /* authoritative */
  if (auth)
    header->hb3 |= HB3_AA;
  
  /* truncation */
  if (trunc)
    {
      header->hb3 |= HB3_TC;
      if (!(ansp = skip_questions(header, qlen)))
	return 0; /* bad packet */
      anscount = authcount = 0;
      log_query(F_AUTH, "reply", NULL, "truncated", 0);
    }
  
  if ((auth || local_query) && nxdomain)
    SET_RCODE(header, NXDOMAIN);
  else
    SET_RCODE(header, NOERROR); /* no error */
  
  header->ancount = htons(anscount);
  header->nscount = htons(authcount);
  header->arcount = htons(0);

  if (!local_query && out_of_zone)
    {
      SET_RCODE(header, REFUSED); 
      header->ancount = htons(0);
      header->nscount = htons(0);
      addr.log.rcode = REFUSED;
      addr.log.ede = EDE_NOT_AUTH;
      log_query(F_UPSTREAM | F_RCODE, "error", &addr, NULL, 0);
      return resize_packet(header,  ansp - (unsigned char *)header, NULL, 0);
    }
  
  return ansp - (unsigned char *)header;
}
  
#endif  
