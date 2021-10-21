/*	$KAME: dhcp6c_script.c,v 1.11 2004/11/28 10:48:38 jinmei Exp $	*/

/*
 * Copyright (C) 2003 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/stat.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <netinet/in.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#include "dhcp6.h"
#include "config.h"
#include "common.h"

static char sipserver_str[] = "new_sip_servers";
static char sipname_str[] = "new_sip_name";
static char dnsserver_str[] = "new_domain_name_servers";
static char dnsname_str[] = "new_domain_name";
static char ntpserver_str[] = "new_ntp_servers";
static char nisserver_str[] = "new_nis_servers";
static char nisname_str[] = "new_nis_name";
static char nispserver_str[] = "new_nisp_servers";
static char nispname_str[] = "new_nisp_name";
static char bcmcsserver_str[] = "new_bcmcs_servers";
static char bcmcsname_str[] = "new_bcmcs_name";
static char aftrname_str[] = "new_aftr_name";//rbj

#define ELBOX_EXPORT_IPV6_ADDRESS
#ifdef ELBOX_EXPORT_IPV6_ADDRESS

static char addr_str[]      = "new_addr";
static char pd_prefix_str[] = "new_pd_prefix";
static char pd_plen_str[]   = "new_pd_plen";
static char pd_pltime_str[]   = "new_pd_pltime"; //add by rbj for IOL test
static char pd_vltime_str[]   = "new_pd_vltime"; //add by rbj for IOL test

#ifdef DO_DLOG
#include <stdarg.h>
#define dlog(args...) _dlog_(__FILE__,__FUNCTION__,__LINE__,##args)
void _dlog_(const char *file, const char *func, int line, const char * format, ...)
{
	char *buf;
	int fd,n;
	va_list list;
	fd = open("/dev/console", O_RDWR);
	buf = (char *)calloc(1,1024);
	va_start(list, format);
	sprintf(buf,"[%s:%s:%d]\n\t",file,func,line);
	vsprintf(buf+strlen(buf), format, list);
	sprintf(buf+strlen(buf), "%c", '\n');
	n = write(fd,buf,strlen(buf));
	va_end(list);
	free(buf);
	close(fd);
}
#else
#define dlog(args...)
#endif

#endif

int
client6_script(scriptpath, state, optinfo)
	char *scriptpath;
	int state;
	struct dhcp6_optinfo *optinfo;
{
	int i, dnsservers, ntpservers, dnsnamelen, envc, elen, ret = 0;
	int sipservers, sipnamelen;
	int nisservers, nisnamelen;
	int nispservers, nispnamelen;
	int bcmcsservers, bcmcsnamelen;
	int aftrservers, aftrnamelen; //rbj
	char **envp, *s;
	char reason[] = "REASON=NBI";
	struct dhcp6_listval *v;
	pid_t pid, wpid;
#ifdef ELBOX_EXPORT_IPV6_ADDRESS
	struct dhcp6_listval *sv;
	char addr[48], pd_prefix[48], pd_plen[3];
	char pd_lft[10];//IOL test
	int addrs = 0, addrlen = 0;
	int pd_prefixs = 0, pd_prelen = 0;
	int pd_plens = 0, pd_plenlen = 0;
#endif
	dlog("---run script---");

	/* if a script is not specified, do nothing */
	if (scriptpath == NULL || strlen(scriptpath) == 0)
		return -1;

	/* initialize counters */
	dnsservers = 0;
	ntpservers = 0;
	dnsnamelen = 0;
	sipservers = 0;
	sipnamelen = 0;
	nisservers = 0;
	nisnamelen = 0;
	nispservers = 0;
	nispnamelen = 0;
	bcmcsservers = 0;
	bcmcsnamelen = 0;
	aftrservers = 0;//rbj
	aftrnamelen = 0;//rbj
	envc = 2;     /* we at least include the reason and the terminator */

	/* count the number of variables */
	for (v = TAILQ_FIRST(&optinfo->dns_list); v; v = TAILQ_NEXT(v, link))
		dnsservers++;
	envc += dnsservers ? 1 : 0;
	for (v = TAILQ_FIRST(&optinfo->dnsname_list); v;
	    v = TAILQ_NEXT(v, link)) {
		dnsnamelen += v->val_vbuf.dv_len;
	}
	envc += dnsnamelen ? 1 : 0;
	for (v = TAILQ_FIRST(&optinfo->ntp_list); v; v = TAILQ_NEXT(v, link))
		ntpservers++;
	envc += ntpservers ? 1 : 0;
	for (v = TAILQ_FIRST(&optinfo->sip_list); v; v = TAILQ_NEXT(v, link))
		sipservers++;
	envc += sipservers ? 1 : 0;
	for (v = TAILQ_FIRST(&optinfo->sipname_list); v;
	    v = TAILQ_NEXT(v, link)) {
		sipnamelen += v->val_vbuf.dv_len;
	}
	envc += sipnamelen ? 1 : 0;

	for (v = TAILQ_FIRST(&optinfo->nis_list); v; v = TAILQ_NEXT(v, link))
		nisservers++;
	envc += nisservers ? 1 : 0;
	for (v = TAILQ_FIRST(&optinfo->nisname_list); v;
	    v = TAILQ_NEXT(v, link)) {
		nisnamelen += v->val_vbuf.dv_len;
	}
	envc += nisnamelen ? 1 : 0;

	for (v = TAILQ_FIRST(&optinfo->nisp_list); v; v = TAILQ_NEXT(v, link))
		nispservers++;
	envc += nispservers ? 1 : 0;
	for (v = TAILQ_FIRST(&optinfo->nispname_list); v;
	    v = TAILQ_NEXT(v, link)) {
		nispnamelen += v->val_vbuf.dv_len;
	}
	envc += nispnamelen ? 1 : 0;

	for (v = TAILQ_FIRST(&optinfo->bcmcs_list); v; v = TAILQ_NEXT(v, link))
		bcmcsservers++;
	envc += bcmcsservers ? 1 : 0;
	for (v = TAILQ_FIRST(&optinfo->bcmcsname_list); v;
	    v = TAILQ_NEXT(v, link)) {
		bcmcsnamelen += v->val_vbuf.dv_len;
	}
	envc += bcmcsnamelen ? 1 : 0;
	
	//rbj
	for (v = TAILQ_FIRST(&optinfo->aftr_list); v; v = TAILQ_NEXT(v, link))
		aftrservers++;
	envc += aftrservers ? 1 : 0;
	for (v = TAILQ_FIRST(&optinfo->aftrname_list); v;
	    v = TAILQ_NEXT(v, link)) {
		aftrnamelen += v->val_vbuf.dv_len;
	}
	envc += aftrnamelen ? 1 : 0;

#ifdef ELBOX_EXPORT_IPV6_ADDRESS
	dlog("envc: %d", envc);
	for (v = TAILQ_FIRST(&optinfo->iana_list); v; v = TAILQ_NEXT(v, link))
		for (sv = TAILQ_FIRST(&v->sublist); sv; sv = TAILQ_NEXT(sv, link))
			addrs++;
	envc += addrs ? 1 : 0;
	if (addrs) addrlen = sizeof(addr) * addrs;
	envc += addrlen ? 1 : 0;

	dlog("envc: %d\taddrs: %d\taddrlen: %d", envc, addrs, addrlen);

	for (v = TAILQ_FIRST(&optinfo->iapd_list); v; v = TAILQ_NEXT(v, link))
		for (sv = TAILQ_FIRST(&v->sublist); sv; sv = TAILQ_NEXT(sv, link))
		{
			pd_prefixs++;
			pd_plens++;
		}
	envc += pd_prefixs ? 2 : 0;
	if (pd_prefixs)
	{
		pd_prelen = sizeof(pd_prefix) * pd_prefixs;
		pd_plenlen = sizeof(pd_plen) * pd_plens;
	}
	envc += pd_prelen ? 2 : 0;
	dlog("envc: %d\tpd_prefixs: %d\tpd_plen: %d", envc, pd_prefixs, pd_prelen);

	envc +=2; //IOL test, save vltime and pltime
#endif

	/* allocate an environments array */
	if ((envp = malloc(sizeof (char *) * envc)) == NULL) {
		debug_printf(LOG_NOTICE, FNAME,
		    "failed to allocate environment buffer");
		return -1;
	}
	memset(envp, 0, sizeof (char *) * envc);

	/*
	 * Copy the parameters as environment variables
	 */
	i = 0;
	/* reason */
	if ((envp[i++] = strdup(reason)) == NULL) {
		debug_printf(LOG_NOTICE, FNAME,
		    "failed to allocate reason strings");
		ret = -1;
		goto clean;
	}
	/* "var=addr1 addr2 ... addrN" + null char for termination */
	if (dnsservers) {
		elen = sizeof (dnsserver_str) +
		    (INET6_ADDRSTRLEN + 1) * dnsservers + 1;
		if ((s = envp[i++] = malloc(elen)) == NULL) {
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for DNS servers");
			ret = -1;
			goto clean;
		}
		memset(s, 0, elen);
		snprintf(s, elen, "%s=", dnsserver_str);
		for (v = TAILQ_FIRST(&optinfo->dns_list); v;
		    v = TAILQ_NEXT(v, link)) {
			char *addr;

			addr = in6addr2str(&v->val_addr6, 0);
			strlcat(s, addr, elen);
			strlcat(s, " ", elen);
		}
	}
	if (ntpservers) {
		elen = sizeof (ntpserver_str) +
		    (INET6_ADDRSTRLEN + 1) * ntpservers + 1;
		if ((s = envp[i++] = malloc(elen)) == NULL) {
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for NTP servers");
			ret = -1;
			goto clean;
		}
		memset(s, 0, elen);
		snprintf(s, elen, "%s=", ntpserver_str);
		for (v = TAILQ_FIRST(&optinfo->ntp_list); v;
		    v = TAILQ_NEXT(v, link)) {
			char *addr;

			addr = in6addr2str(&v->val_addr6, 0);
			strlcat(s, addr, elen);
			strlcat(s, " ", elen);
		}
	}

	if (dnsnamelen) {
		elen = sizeof (dnsname_str) + dnsnamelen + 1;
		if ((s = envp[i++] = malloc(elen)) == NULL) {
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for DNS name");
			ret = -1;
			goto clean;
		}
		memset(s, 0, elen);
		snprintf(s, elen, "%s=", dnsname_str);
		for (v = TAILQ_FIRST(&optinfo->dnsname_list); v;
		    v = TAILQ_NEXT(v, link)) {
			strlcat(s, v->val_vbuf.dv_buf, elen);
			strlcat(s, " ", elen);
		}
	}

	if (sipservers) {
		elen = sizeof (sipserver_str) +
		    (INET6_ADDRSTRLEN + 1) * sipservers + 1;
		if ((s = envp[i++] = malloc(elen)) == NULL) {
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for SIP servers");
			ret = -1;
			goto clean;
		}
		memset(s, 0, elen);
		snprintf(s, elen, "%s=", sipserver_str);
		for (v = TAILQ_FIRST(&optinfo->sip_list); v;
		    v = TAILQ_NEXT(v, link)) {
			char *addr;

			addr = in6addr2str(&v->val_addr6, 0);
			strlcat(s, addr, elen);
			strlcat(s, " ", elen);
		}
	}
	if (sipnamelen) {
		elen = sizeof (sipname_str) + sipnamelen + 1;
		if ((s = envp[i++] = malloc(elen)) == NULL) {
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for SIP domain name");
			ret = -1;
			goto clean;
		}
		memset(s, 0, elen);
		snprintf(s, elen, "%s=", sipname_str);
		for (v = TAILQ_FIRST(&optinfo->sipname_list); v;
		    v = TAILQ_NEXT(v, link)) {
			strlcat(s, v->val_vbuf.dv_buf, elen);
			strlcat(s, " ", elen);
		}
	}

	if (nisservers) {
		elen = sizeof (nisserver_str) +
		    (INET6_ADDRSTRLEN + 1) * nisservers + 1;
		if ((s = envp[i++] = malloc(elen)) == NULL) {
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for NIS servers");
			ret = -1;
			goto clean;
		}
		memset(s, 0, elen);
		snprintf(s, elen, "%s=", nisserver_str);
		for (v = TAILQ_FIRST(&optinfo->nis_list); v;
		    v = TAILQ_NEXT(v, link)) {
			char *addr;

			addr = in6addr2str(&v->val_addr6, 0);
			strlcat(s, addr, elen);
			strlcat(s, " ", elen);
		}
	}
	if (nisnamelen) {
		elen = sizeof (nisname_str) + nisnamelen + 1;
		if ((s = envp[i++] = malloc(elen)) == NULL) {
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for NIS domain name");
			ret = -1;
			goto clean;
		}
		memset(s, 0, elen);
		snprintf(s, elen, "%s=", nisname_str);
		for (v = TAILQ_FIRST(&optinfo->nisname_list); v;
		    v = TAILQ_NEXT(v, link)) {
			strlcat(s, v->val_vbuf.dv_buf, elen);
			strlcat(s, " ", elen);
		}
	}

	if (nispservers) {
		elen = sizeof (nispserver_str) +
		    (INET6_ADDRSTRLEN + 1) * nispservers + 1;
		if ((s = envp[i++] = malloc(elen)) == NULL) {
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for NIS+ servers");
			ret = -1;
			goto clean;
		}
		memset(s, 0, elen);
		snprintf(s, elen, "%s=", nispserver_str);
		for (v = TAILQ_FIRST(&optinfo->nisp_list); v;
		    v = TAILQ_NEXT(v, link)) {
			char *addr;

			addr = in6addr2str(&v->val_addr6, 0);
			strlcat(s, addr, elen);
			strlcat(s, " ", elen);
		}
	}
	if (nispnamelen) {
		elen = sizeof (nispname_str) + nispnamelen + 1;
		if ((s = envp[i++] = malloc(elen)) == NULL) {
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for NIS+ domain name");
			ret = -1;
			goto clean;
		}
		memset(s, 0, elen);
		snprintf(s, elen, "%s=", nispname_str);
		for (v = TAILQ_FIRST(&optinfo->nispname_list); v;
		    v = TAILQ_NEXT(v, link)) {
			strlcat(s, v->val_vbuf.dv_buf, elen);
			strlcat(s, " ", elen);
		}
	}

	if (bcmcsservers) {
		elen = sizeof (bcmcsserver_str) +
		    (INET6_ADDRSTRLEN + 1) * bcmcsservers + 1;
		if ((s = envp[i++] = malloc(elen)) == NULL) {
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for BCMC servers");
			ret = -1;
			goto clean;
		}
		memset(s, 0, elen);
		snprintf(s, elen, "%s=", bcmcsserver_str);
		for (v = TAILQ_FIRST(&optinfo->bcmcs_list); v;
		    v = TAILQ_NEXT(v, link)) {
			char *addr;

			addr = in6addr2str(&v->val_addr6, 0);
			strlcat(s, addr, elen);
			strlcat(s, " ", elen);
		}
	}
	if (bcmcsnamelen) {
		elen = sizeof (bcmcsname_str) + bcmcsnamelen + 1;
		if ((s = envp[i++] = malloc(elen)) == NULL) {
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for BCMC domain name");
			ret = -1;
			goto clean;
		}
		memset(s, 0, elen);
		snprintf(s, elen, "%s=", bcmcsname_str);
		for (v = TAILQ_FIRST(&optinfo->bcmcsname_list); v;
		    v = TAILQ_NEXT(v, link)) {
			strlcat(s, v->val_vbuf.dv_buf, elen);
			strlcat(s, " ", elen);
		}
	}
	
	//rbj
	if (aftrnamelen) {
		elen = sizeof (aftrname_str) + aftrnamelen + 1;
		if ((s = envp[i++] = malloc(elen)) == NULL) {
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for AFTR domain name");
			ret = -1;
			goto clean;
		}
		memset(s, 0, elen);
		snprintf(s, elen, "%s=", aftrname_str);
		//for (v = TAILQ_FIRST(&optinfo->aftrname_list); v;
		//    v = TAILQ_NEXT(v, link)) {
		//	strlcat(s, v->val_vbuf.dv_buf, elen);
		//	strlcat(s, " ", elen);
		//}
		//only take first one
		v = TAILQ_FIRST(&optinfo->aftrname_list);
		strlcat(s, v->val_vbuf.dv_buf, elen);
		dlog("aftr server: %s", s);
	}

#ifdef ELBOX_EXPORT_IPV6_ADDRESS
	if (addrs)
	{
		elen = sizeof (addr_str) + addrlen + 1;
		if ((s = envp[i++] = malloc(elen)) == NULL)
		{
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for address");
			ret = -1;
			goto clean;
		}
		memset(s, 0, elen);
		snprintf(s, elen, "%s=", addr_str);
		for (v = TAILQ_FIRST(&optinfo->iana_list); v; v = TAILQ_NEXT(v, link))
		{
			dlog("v->type: %d", v->type);

			for (sv = TAILQ_FIRST(&v->sublist); sv; sv = TAILQ_NEXT(sv, link))
			{
				dlog("sv->type: %d", sv->type);

				if (sv->type == DHCP6_LISTVAL_STATEFULADDR6)
				{
					snprintf(addr, sizeof(addr), "%s", in6addr2str(&sv->val_statefuladdr6.addr, 0));
					dlog("addr: %s", addr);
					strlcat(s, addr, elen);
					strlcat(s, " ", elen);
				}
			}
		}
	}

	dlog("pd_prefixs: %d", pd_prefixs);
	if (pd_prefixs)
	{
		int elen2;
		int elen3; //add by rbj for IOL test to save lifetime info
		int elen4; //add by rbj for IOL test to save lifetime info
		char *s2;
		char *s3;  //add by rbj for IOL test to save lifetime info
		char *s4;  //add by rbj for IOL test to save lifetime info

		elen  = sizeof(pd_prefix_str) + pd_prelen + 1;
		elen2 = sizeof(pd_plen_str) + pd_plenlen + 1;
		elen3 = sizeof(pd_pltime_str) + sizeof(pd_lft)*1 + 1;  //add by rbj for IOL test
		elen4 = sizeof(pd_vltime_str) + sizeof(pd_lft)*1 + 1;  //add by rbj for IOL test

		if ((s = envp[i++] = malloc(elen)) == NULL)
		{
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for pd prefix");
			ret = -1;
			goto clean;
		}
		if ((s2 = envp[i++] = malloc(elen2)) == NULL)
		{
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for pd prefix length");
			ret = -1;
			goto clean;
		}

		//IOL test
		//>>>>
		if ((s3 = envp[i++] = malloc(elen3)) == NULL)
		{
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for pd pltime");
			ret = -1;
			goto clean;
		}
		if ((s4 = envp[i++] = malloc(elen4)) == NULL)
		{
			debug_printf(LOG_NOTICE, FNAME,
			    "failed to allocate strings for pd vltime");
			ret = -1;
			goto clean;
		}
		memset(s3, 0, elen3);
		memset(s4, 0, elen4);
		snprintf(s3, elen3, "%s=", pd_pltime_str);
		snprintf(s4, elen4, "%s=", pd_vltime_str);
		//<<<<

		memset(s, 0, elen);
		memset(s2, 0, elen2);
		snprintf(s, elen, "%s=", pd_prefix_str);
		snprintf(s2, elen2, "%s=", pd_plen_str);
		for (v = TAILQ_FIRST(&optinfo->iapd_list); v; v = TAILQ_NEXT(v, link))
		{
			dlog("[pd]v->type: %d", v->type);

			for (sv = TAILQ_FIRST(&v->sublist); sv; sv = TAILQ_NEXT(sv, link))
			{
				dlog("[pd]sv->type: %d", sv->type);

				if (sv->type == DHCP6_LISTVAL_PREFIX6)
				{
					dlog("vltime : %d\n",sv->val_prefix6.vltime);
					if(sv->val_prefix6.vltime != 0)
					{
						snprintf(pd_prefix, sizeof(pd_prefix), "%s", in6addr2str(&sv->val_prefix6.addr, 0));
						dlog("pd_prefix: %s", pd_prefix);
						strlcat(s, pd_prefix, elen);
						strlcat(s, " ", elen);

						snprintf(pd_plen, sizeof(pd_plen), "%d", sv->val_prefix6.plen);
						dlog("pd_plen: %s", pd_plen);
						strlcat(s2, pd_plen, elen2);
						strlcat(s2, " ", elen2);
					}
				}
			}
		}
		
		//IOL test
		for (v = TAILQ_FIRST(&optinfo->iapd_list); v; v = TAILQ_NEXT(v, link))
		{
			int found = 0;
			dlog("[pd]v->type: %d", v->type);

			for (sv = TAILQ_FIRST(&v->sublist); sv; sv = TAILQ_NEXT(sv, link))
			{
				dlog("[pd]sv->type: %d", sv->type);

				if (sv->type == DHCP6_LISTVAL_PREFIX6)
				{
					dlog("vltime : %d\n",sv->val_prefix6.vltime);
					if(sv->val_prefix6.vltime != 0)
					{
						dlog("pltime : %d\n",sv->val_prefix6.pltime);
						snprintf(pd_lft, sizeof(pd_lft), "%d", sv->val_prefix6.pltime);
						strlcat(s3, pd_lft, elen3);
						dlog("vltime : %d\n",sv->val_prefix6.vltime);
						snprintf(pd_lft, sizeof(pd_lft), "%d", sv->val_prefix6.vltime);
						strlcat(s4, pd_lft, elen3);
						found = 1;
					}
				}
			}
			if(found) break;
		}
	}
#endif
	//rbj for test
#if 0
	int j=0;
	dlog("Output envp: \n");
	for(j=0;j<envc;j++)
	{
		dlog("%s\n",envp[j]);
	}
#endif
	/* launch the script */
	pid = fork();
	if (pid < 0) {
		debug_printf(LOG_ERR, FNAME, "failed to fork: %s", strerror(errno));
		ret = -1;
		goto clean;
	} else if (pid) {
		int wstatus;

		do {
			wpid = wait(&wstatus);
		} while (wpid != pid && wpid > 0);

		if (wpid < 0)
			debug_printf(LOG_ERR, FNAME, "wait: %s", strerror(errno));
		else {
			debug_printf(LOG_DEBUG, FNAME,
			    "script \"%s\" terminated", scriptpath);
		}
	} else {
		char *argv[2];
		int fd;

		argv[0] = scriptpath;
		argv[1] = NULL;

		if (safefile(scriptpath)) {
			debug_printf(LOG_ERR, FNAME,
			    "script \"%s\" cannot be executed safely",
			    scriptpath);
			exit(1);
		}

		if (foreground == 0 && (fd = open("/dev/null", O_RDWR)) != -1) {
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			if (fd > STDERR_FILENO)
				close(fd);
		}

		execve(scriptpath, argv, envp);

		debug_printf(LOG_ERR, FNAME, "child: exec failed: %s",
		    strerror(errno));
		exit(0);
	}

  clean:
	for (i = 0; i < envc; i++)
		free(envp[i]);
	free(envp);

	return ret;
}
