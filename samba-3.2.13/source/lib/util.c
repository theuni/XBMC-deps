/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Jeremy Allison 2001-2007
   Copyright (C) Simo Sorce 2001
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2003
   Copyright (C) James Peach 2006

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"

extern char *global_clobber_region_function;
extern unsigned int global_clobber_region_line;

/* Max allowable allococation - 256mb - 0x10000000 */
#define MAX_ALLOC_SIZE (1024*1024*256)

#if (defined(HAVE_NETGROUP) && defined (WITH_AUTOMOUNT))
#ifdef WITH_NISPLUS_HOME
#ifdef BROKEN_NISPLUS_INCLUDE_FILES
/*
 * The following lines are needed due to buggy include files
 * in Solaris 2.6 which define GROUP in both /usr/include/sys/acl.h and
 * also in /usr/include/rpcsvc/nis.h. The definitions conflict. JRA.
 * Also GROUP_OBJ is defined as 0x4 in /usr/include/sys/acl.h and as
 * an enum in /usr/include/rpcsvc/nis.h.
 */

#if defined(GROUP)
#undef GROUP
#endif

#if defined(GROUP_OBJ)
#undef GROUP_OBJ
#endif

#endif /* BROKEN_NISPLUS_INCLUDE_FILES */

#include <rpcsvc/nis.h>

#endif /* WITH_NISPLUS_HOME */
#endif /* HAVE_NETGROUP && WITH_AUTOMOUNT */

enum protocol_types Protocol = PROTOCOL_COREPLUS;

/* this is used by the chaining code */
int chain_size = 0;

int trans_num = 0;

static enum remote_arch_types ra_type = RA_UNKNOWN;

/***********************************************************************
 Definitions for all names.
***********************************************************************/

static char *smb_myname;
static char *smb_myworkgroup;
static char *smb_scope;
static int smb_num_netbios_names;
static char **smb_my_netbios_names;

/***********************************************************************
 Allocate and set myname. Ensure upper case.
***********************************************************************/

bool set_global_myname(const char *myname)
{
	SAFE_FREE(smb_myname);
	smb_myname = SMB_STRDUP(myname);
	if (!smb_myname)
		return False;
	strupper_m(smb_myname);
	return True;
}

const char *global_myname(void)
{
	return smb_myname;
}

/***********************************************************************
 Allocate and set myworkgroup. Ensure upper case.
***********************************************************************/

bool set_global_myworkgroup(const char *myworkgroup)
{
	SAFE_FREE(smb_myworkgroup);
	smb_myworkgroup = SMB_STRDUP(myworkgroup);
	if (!smb_myworkgroup)
		return False;
	strupper_m(smb_myworkgroup);
	return True;
}

const char *lp_workgroup(void)
{
	return smb_myworkgroup;
}

/***********************************************************************
 Allocate and set scope. Ensure upper case.
***********************************************************************/

bool set_global_scope(const char *scope)
{
	SAFE_FREE(smb_scope);
	smb_scope = SMB_STRDUP(scope);
	if (!smb_scope)
		return False;
	strupper_m(smb_scope);
	return True;
}

/*********************************************************************
 Ensure scope is never null string.
*********************************************************************/

const char *global_scope(void)
{
	if (!smb_scope)
		set_global_scope("");
	return smb_scope;
}

static void free_netbios_names_array(void)
{
	int i;

	for (i = 0; i < smb_num_netbios_names; i++)
		SAFE_FREE(smb_my_netbios_names[i]);

	SAFE_FREE(smb_my_netbios_names);
	smb_num_netbios_names = 0;
}

static bool allocate_my_netbios_names_array(size_t number)
{
	free_netbios_names_array();

	smb_num_netbios_names = number + 1;
	smb_my_netbios_names = SMB_MALLOC_ARRAY( char *, smb_num_netbios_names );

	if (!smb_my_netbios_names)
		return False;

	memset(smb_my_netbios_names, '\0', sizeof(char *) * smb_num_netbios_names);
	return True;
}

static bool set_my_netbios_names(const char *name, int i)
{
	SAFE_FREE(smb_my_netbios_names[i]);

	smb_my_netbios_names[i] = SMB_STRDUP(name);
	if (!smb_my_netbios_names[i])
		return False;
	strupper_m(smb_my_netbios_names[i]);
	return True;
}

/***********************************************************************
 Free memory allocated to global objects
***********************************************************************/

void gfree_names(void)
{
	SAFE_FREE( smb_myname );
	SAFE_FREE( smb_myworkgroup );
	SAFE_FREE( smb_scope );
	free_netbios_names_array();
	free_local_machine_name();
}

void gfree_all( void )
{
	gfree_names();
	gfree_loadparm();
	gfree_case_tables();
	gfree_charcnv();
	gfree_interfaces();
	gfree_debugsyms();
}

const char *my_netbios_names(int i)
{
	return smb_my_netbios_names[i];
}

bool set_netbios_aliases(const char **str_array)
{
	size_t namecount;

	/* Work out the max number of netbios aliases that we have */
	for( namecount=0; str_array && (str_array[namecount] != NULL); namecount++ )
		;

	if ( global_myname() && *global_myname())
		namecount++;

	/* Allocate space for the netbios aliases */
	if (!allocate_my_netbios_names_array(namecount))
		return False;

	/* Use the global_myname string first */
	namecount=0;
	if ( global_myname() && *global_myname()) {
		set_my_netbios_names( global_myname(), namecount );
		namecount++;
	}

	if (str_array) {
		size_t i;
		for ( i = 0; str_array[i] != NULL; i++) {
			size_t n;
			bool duplicate = False;

			/* Look for duplicates */
			for( n=0; n<namecount; n++ ) {
				if( strequal( str_array[i], my_netbios_names(n) ) ) {
					duplicate = True;
					break;
				}
			}
			if (!duplicate) {
				if (!set_my_netbios_names(str_array[i], namecount))
					return False;
				namecount++;
			}
		}
	}
	return True;
}

/****************************************************************************
  Common name initialization code.
****************************************************************************/

bool init_names(void)
{
	int n;

	if (global_myname() == NULL || *global_myname() == '\0') {
		if (!set_global_myname(myhostname())) {
			DEBUG( 0, ( "init_structs: malloc fail.\n" ) );
			return False;
		}
	}

	if (!set_netbios_aliases(lp_netbios_aliases())) {
		DEBUG( 0, ( "init_structs: malloc fail.\n" ) );
		return False;
	}

	set_local_machine_name(global_myname(),false);

	DEBUG( 5, ("Netbios name list:-\n") );
	for( n=0; my_netbios_names(n); n++ ) {
		DEBUGADD( 5, ("my_netbios_names[%d]=\"%s\"\n",
					n, my_netbios_names(n) ) );
	}

	return( True );
}

/**************************************************************************n
  Code to cope with username/password auth options from the commandline.
  Used mainly in client tools.
****************************************************************************/

static struct user_auth_info cmdline_auth_info = {
	NULL,	/* username */
	NULL,	/* password */
	false,	/* got_pass */
	false,	/* use_kerberos */
	Undefined, /* signing state */
	false,	/* smb_encrypt */
	false   /* use machine account */
};

const char *get_cmdline_auth_info_username(void)
{
	if (!cmdline_auth_info.username) {
		return "";
	}
	return cmdline_auth_info.username;
}

void set_cmdline_auth_info_username(const char *username)
{
	SAFE_FREE(cmdline_auth_info.username);
	cmdline_auth_info.username = SMB_STRDUP(username);
	if (!cmdline_auth_info.username) {
		exit(ENOMEM);
	}
}

const char *get_cmdline_auth_info_password(void)
{
	if (!cmdline_auth_info.password) {
		return "";
	}
	return cmdline_auth_info.password;
}

void set_cmdline_auth_info_password(const char *password)
{
	SAFE_FREE(cmdline_auth_info.password);
	cmdline_auth_info.password = SMB_STRDUP(password);
	if (!cmdline_auth_info.password) {
		exit(ENOMEM);
	}
	cmdline_auth_info.got_pass = true;
}

bool set_cmdline_auth_info_signing_state(const char *arg)
{
	cmdline_auth_info.signing_state = -1;
	if (strequal(arg, "off") || strequal(arg, "no") ||
			strequal(arg, "false")) {
		cmdline_auth_info.signing_state = false;
	} else if (strequal(arg, "on") || strequal(arg, "yes") ||
			strequal(arg, "true") || strequal(arg, "auto")) {
		cmdline_auth_info.signing_state = true;
	} else if (strequal(arg, "force") || strequal(arg, "required") ||
			strequal(arg, "forced")) {
		cmdline_auth_info.signing_state = Required;
	} else {
		return false;
	}
	return true;
}

int get_cmdline_auth_info_signing_state(void)
{
	return cmdline_auth_info.signing_state;
}

bool get_cmdline_auth_info_use_kerberos(void)
{
	return cmdline_auth_info.use_kerberos;
}

/* This should only be used by lib/popt_common.c JRA */
void set_cmdline_auth_info_use_krb5_ticket(void)
{
	cmdline_auth_info.use_kerberos = true;
	cmdline_auth_info.got_pass = true;
}

/* This should only be used by lib/popt_common.c JRA */
void set_cmdline_auth_info_smb_encrypt(void)
{
	cmdline_auth_info.smb_encrypt = true;
}

void set_cmdline_auth_info_use_machine_account(void)
{
	cmdline_auth_info.use_machine_account = true;
}

bool get_cmdline_auth_info_got_pass(void)
{
	return cmdline_auth_info.got_pass;
}

bool get_cmdline_auth_info_smb_encrypt(void)
{
	return cmdline_auth_info.smb_encrypt;
}

bool get_cmdline_auth_info_use_machine_account(void)
{
	return cmdline_auth_info.use_machine_account;
}

bool get_cmdline_auth_info_copy(struct user_auth_info *info)
{
	*info = cmdline_auth_info;
	/* Now re-alloc the strings. */
	info->username = SMB_STRDUP(get_cmdline_auth_info_username());
	info->password = SMB_STRDUP(get_cmdline_auth_info_password());
	if (!info->username || !info->password) {
		return false;
	}
	return true;
}

bool set_cmdline_auth_info_machine_account_creds(void)
{
	char *pass = NULL;
	char *account = NULL;

	if (!get_cmdline_auth_info_use_machine_account()) {
		return false;
	}

	if (!secrets_init()) {
		d_printf("ERROR: Unable to open secrets database\n");
		return false;
	}

	if (asprintf(&account, "%s$@%s", global_myname(), lp_realm()) < 0) {
		return false;
	}

	pass = secrets_fetch_machine_password(lp_workgroup(), NULL, NULL);
	if (!pass) {
		d_printf("ERROR: Unable to fetch machine password for "
			"%s in domain %s\n",
			account, lp_workgroup());
		SAFE_FREE(account);
		return false;
	}

	set_cmdline_auth_info_username(account);
	set_cmdline_auth_info_password(pass);

	SAFE_FREE(account);
	SAFE_FREE(pass);

	return true;
}

/**************************************************************************n
 Find a suitable temporary directory. The result should be copied immediately
 as it may be overwritten by a subsequent call.
****************************************************************************/

const char *tmpdir(void)
{
	char *p;
	if ((p = getenv("TMPDIR")))
		return p;
	return "/tmp";
}

/****************************************************************************
 Add a gid to an array of gids if it's not already there.
****************************************************************************/

bool add_gid_to_array_unique(TALLOC_CTX *mem_ctx, gid_t gid,
			     gid_t **gids, size_t *num_gids)
{
	int i;

	if ((*num_gids != 0) && (*gids == NULL)) {
		/*
		 * A former call to this routine has failed to allocate memory
		 */
		return False;
	}

	for (i=0; i<*num_gids; i++) {
		if ((*gids)[i] == gid) {
			return True;
		}
	}

	*gids = TALLOC_REALLOC_ARRAY(mem_ctx, *gids, gid_t, *num_gids+1);
	if (*gids == NULL) {
		*num_gids = 0;
		return False;
	}

	(*gids)[*num_gids] = gid;
	*num_gids += 1;
	return True;
}

/****************************************************************************
 Like atoi but gets the value up to the separator character.
****************************************************************************/

static const char *Atoic(const char *p, int *n, const char *c)
{
	if (!isdigit((int)*p)) {
		DEBUG(5, ("Atoic: malformed number\n"));
		return NULL;
	}

	(*n) = atoi(p);

	while ((*p) && isdigit((int)*p))
		p++;

	if (strchr_m(c, *p) == NULL) {
		DEBUG(5, ("Atoic: no separator characters (%s) not found\n", c));
		return NULL;
	}

	return p;
}

/*************************************************************************
 Reads a list of numbers.
 *************************************************************************/

const char *get_numlist(const char *p, uint32 **num, int *count)
{
	int val;

	if (num == NULL || count == NULL)
		return NULL;

	(*count) = 0;
	(*num  ) = NULL;

	while ((p = Atoic(p, &val, ":,")) != NULL && (*p) != ':') {
		*num = SMB_REALLOC_ARRAY((*num), uint32, (*count)+1);
		if (!(*num)) {
			return NULL;
		}
		(*num)[(*count)] = val;
		(*count)++;
		p++;
	}

	return p;
}

/*******************************************************************
 Check if a file exists - call vfs_file_exist for samba files.
********************************************************************/

bool file_exist(const char *fname,SMB_STRUCT_STAT *sbuf)
{
	SMB_STRUCT_STAT st;
	if (!sbuf)
		sbuf = &st;
  
	if (sys_stat(fname,sbuf) != 0) 
		return(False);

	return((S_ISREG(sbuf->st_mode)) || (S_ISFIFO(sbuf->st_mode)));
}

/*******************************************************************
 Check if a unix domain socket exists - call vfs_file_exist for samba files.
********************************************************************/

bool socket_exist(const char *fname)
{
	SMB_STRUCT_STAT st;
	if (sys_stat(fname,&st) != 0) 
		return(False);

	return S_ISSOCK(st.st_mode);
}

/*******************************************************************
 Check a files mod time.
********************************************************************/

time_t file_modtime(const char *fname)
{
	SMB_STRUCT_STAT st;
  
	if (sys_stat(fname,&st) != 0) 
		return(0);

	return(st.st_mtime);
}

/*******************************************************************
 Check if a directory exists.
********************************************************************/

bool directory_exist(char *dname,SMB_STRUCT_STAT *st)
{
	SMB_STRUCT_STAT st2;
	bool ret;

	if (!st)
		st = &st2;

	if (sys_stat(dname,st) != 0) 
		return(False);

	ret = S_ISDIR(st->st_mode);
	if(!ret)
		errno = ENOTDIR;
	return ret;
}

/*******************************************************************
 Returns the size in bytes of the named file.
********************************************************************/

SMB_OFF_T get_file_size(char *file_name)
{
	SMB_STRUCT_STAT buf;
	buf.st_size = 0;
	if(sys_stat(file_name,&buf) != 0)
		return (SMB_OFF_T)-1;
	return(buf.st_size);
}

/*******************************************************************
 Return a string representing an attribute for a file.
********************************************************************/

char *attrib_string(uint16 mode)
{
	fstring attrstr;

	attrstr[0] = 0;

	if (mode & aVOLID) fstrcat(attrstr,"V");
	if (mode & aDIR) fstrcat(attrstr,"D");
	if (mode & aARCH) fstrcat(attrstr,"A");
	if (mode & aHIDDEN) fstrcat(attrstr,"H");
	if (mode & aSYSTEM) fstrcat(attrstr,"S");
	if (mode & aRONLY) fstrcat(attrstr,"R");	  

	return talloc_strdup(talloc_tos(), attrstr);
}

/*******************************************************************
 Show a smb message structure.
********************************************************************/

void show_msg(char *buf)
{
	int i;
	int bcc=0;

	if (!DEBUGLVL(5))
		return;
	
	DEBUG(5,("size=%d\nsmb_com=0x%x\nsmb_rcls=%d\nsmb_reh=%d\nsmb_err=%d\nsmb_flg=%d\nsmb_flg2=%d\n",
			smb_len(buf),
			(int)CVAL(buf,smb_com),
			(int)CVAL(buf,smb_rcls),
			(int)CVAL(buf,smb_reh),
			(int)SVAL(buf,smb_err),
			(int)CVAL(buf,smb_flg),
			(int)SVAL(buf,smb_flg2)));
	DEBUGADD(5,("smb_tid=%d\nsmb_pid=%d\nsmb_uid=%d\nsmb_mid=%d\n",
			(int)SVAL(buf,smb_tid),
			(int)SVAL(buf,smb_pid),
			(int)SVAL(buf,smb_uid),
			(int)SVAL(buf,smb_mid)));
	DEBUGADD(5,("smt_wct=%d\n",(int)CVAL(buf,smb_wct)));

	for (i=0;i<(int)CVAL(buf,smb_wct);i++)
		DEBUGADD(5,("smb_vwv[%2d]=%5d (0x%X)\n",i,
			SVAL(buf,smb_vwv+2*i),SVAL(buf,smb_vwv+2*i)));
	
	bcc = (int)SVAL(buf,smb_vwv+2*(CVAL(buf,smb_wct)));

	DEBUGADD(5,("smb_bcc=%d\n",bcc));

	if (DEBUGLEVEL < 10)
		return;

	if (DEBUGLEVEL < 50)
		bcc = MIN(bcc, 512);

	dump_data(10, (uint8 *)smb_buf(buf), bcc);	
}

/*******************************************************************
 Set the length and marker of an encrypted smb packet.
********************************************************************/

void smb_set_enclen(char *buf,int len,uint16 enc_ctx_num)
{
	_smb_setlen(buf,len);

	SCVAL(buf,4,0xFF);
	SCVAL(buf,5,'E');
	SSVAL(buf,6,enc_ctx_num);
}

/*******************************************************************
 Set the length and marker of an smb packet.
********************************************************************/

void smb_setlen(char *buf,int len)
{
	_smb_setlen(buf,len);

	SCVAL(buf,4,0xFF);
	SCVAL(buf,5,'S');
	SCVAL(buf,6,'M');
	SCVAL(buf,7,'B');
}

/*******************************************************************
 Setup only the byte count for a smb message.
********************************************************************/

int set_message_bcc(char *buf,int num_bytes)
{
	int num_words = CVAL(buf,smb_wct);
	SSVAL(buf,smb_vwv + num_words*SIZEOFWORD,num_bytes);
	_smb_setlen(buf,smb_size + num_words*2 + num_bytes - 4);
	return (smb_size + num_words*2 + num_bytes);
}

/*******************************************************************
 Add a data blob to the end of a smb_buf, adjusting bcc and smb_len.
 Return the bytes added
********************************************************************/

ssize_t message_push_blob(uint8 **outbuf, DATA_BLOB blob)
{
	size_t newlen = smb_len(*outbuf) + 4 + blob.length;
	uint8 *tmp;

	if (!(tmp = TALLOC_REALLOC_ARRAY(NULL, *outbuf, uint8, newlen))) {
		DEBUG(0, ("talloc failed\n"));
		return -1;
	}
	*outbuf = tmp;

	memcpy(tmp + smb_len(tmp) + 4, blob.data, blob.length);
	set_message_bcc((char *)tmp, smb_buflen(tmp) + blob.length);
	return blob.length;
}

/*******************************************************************
 Reduce a file name, removing .. elements.
********************************************************************/

static char *dos_clean_name(TALLOC_CTX *ctx, const char *s)
{
	char *p = NULL;
	char *str = NULL;

	DEBUG(3,("dos_clean_name [%s]\n",s));

	/* remove any double slashes */
	str = talloc_all_string_sub(ctx, s, "\\\\", "\\");
	if (!str) {
		return NULL;
	}

	/* Remove leading .\\ characters */
	if(strncmp(str, ".\\", 2) == 0) {
		trim_string(str, ".\\", NULL);
		if(*str == 0) {
			str = talloc_strdup(ctx, ".\\");
			if (!str) {
				return NULL;
			}
		}
	}

	while ((p = strstr_m(str,"\\..\\")) != NULL) {
		char *s1;

		*p = 0;
		s1 = p+3;

		if ((p=strrchr_m(str,'\\')) != NULL) {
			*p = 0;
		} else {
			*str = 0;
		}
		str = talloc_asprintf(ctx,
				"%s%s",
				str,
				s1);
		if (!str) {
			return NULL;
		}
	}

	trim_string(str,NULL,"\\..");
	return talloc_all_string_sub(ctx, str, "\\.\\", "\\");
}

/*******************************************************************
 Reduce a file name, removing .. elements.
********************************************************************/

char *unix_clean_name(TALLOC_CTX *ctx, const char *s)
{
	char *p = NULL;
	char *str = NULL;

	DEBUG(3,("unix_clean_name [%s]\n",s));

	/* remove any double slashes */
	str = talloc_all_string_sub(ctx, s, "//","/");
	if (!str) {
		return NULL;
	}

	/* Remove leading ./ characters */
	if(strncmp(str, "./", 2) == 0) {
		trim_string(str, "./", NULL);
		if(*str == 0) {
			str = talloc_strdup(ctx, "./");
			if (!str) {
				return NULL;
			}
		}
	}

	while ((p = strstr_m(str,"/../")) != NULL) {
		char *s1;

		*p = 0;
		s1 = p+3;

		if ((p=strrchr_m(str,'/')) != NULL) {
			*p = 0;
		} else {
			*str = 0;
		}
		str = talloc_asprintf(ctx,
				"%s%s",
				str,
				s1);
		if (!str) {
			return NULL;
		}
	}

	trim_string(str,NULL,"/..");
	return talloc_all_string_sub(ctx, str, "/./", "/");
}

char *clean_name(TALLOC_CTX *ctx, const char *s)
{
	char *str = dos_clean_name(ctx, s);
	if (!str) {
		return NULL;
	}
	return unix_clean_name(ctx, str);
}

/*******************************************************************
 Close the low 3 fd's and open dev/null in their place.
********************************************************************/

void close_low_fds(bool stderr_too)
{
#ifndef VALGRIND
	int fd;
	int i;

	close(0);
	close(1);

	if (stderr_too)
		close(2);

	/* try and use up these file descriptors, so silly
		library routines writing to stdout etc won't cause havoc */
	for (i=0;i<3;i++) {
		if (i == 2 && !stderr_too)
			continue;

		fd = sys_open("/dev/null",O_RDWR,0);
		if (fd < 0)
			fd = sys_open("/dev/null",O_WRONLY,0);
		if (fd < 0) {
			DEBUG(0,("Can't open /dev/null\n"));
			return;
		}
		if (fd != i) {
			DEBUG(0,("Didn't get file descriptor %d\n",i));
			return;
		}
	}
#endif
}

/*******************************************************************
 Write data into an fd at a given offset. Ignore seek errors.
********************************************************************/

ssize_t write_data_at_offset(int fd, const char *buffer, size_t N, SMB_OFF_T pos)
{
	size_t total=0;
	ssize_t ret;

	if (pos == (SMB_OFF_T)-1) {
		return write_data(fd, buffer, N);
	}
#if defined(HAVE_PWRITE) || defined(HAVE_PRWITE64)
	while (total < N) {
		ret = sys_pwrite(fd,buffer + total,N - total, pos);
		if (ret == -1 && errno == ESPIPE) {
			return write_data(fd, buffer + total,N - total);
		}
		if (ret == -1) {
			DEBUG(0,("write_data_at_offset: write failure. Error = %s\n", strerror(errno) ));
			return -1;
		}
		if (ret == 0) {
			return total;
		}
		total += ret;
		pos += ret;
	}
	return (ssize_t)total;
#else
	/* Use lseek and write_data. */
	if (sys_lseek(fd, pos, SEEK_SET) == -1) {
		if (errno != ESPIPE) {
			return -1;
		}
	}
	return write_data(fd, buffer, N);
#endif
}

/****************************************************************************
 Set a fd into blocking/nonblocking mode. Uses POSIX O_NONBLOCK if available,
 else
  if SYSV use O_NDELAY
  if BSD use FNDELAY
****************************************************************************/

int set_blocking(int fd, bool set)
{
	int val;
#ifdef O_NONBLOCK
#define FLAG_TO_SET O_NONBLOCK
#else
#ifdef SYSV
#define FLAG_TO_SET O_NDELAY
#else /* BSD */
#define FLAG_TO_SET FNDELAY
#endif
#endif

	if((val = sys_fcntl_long(fd, F_GETFL, 0)) == -1)
		return -1;
	if(set) /* Turn blocking on - ie. clear nonblock flag */
		val &= ~FLAG_TO_SET;
	else
		val |= FLAG_TO_SET;
	return sys_fcntl_long( fd, F_SETFL, val);
#undef FLAG_TO_SET
}

/*******************************************************************
 Sleep for a specified number of milliseconds.
********************************************************************/

void smb_msleep(unsigned int t)
{
#if defined(HAVE_NANOSLEEP)
	struct timespec tval;
	int ret;

	tval.tv_sec = t/1000;
	tval.tv_nsec = 1000000*(t%1000);

	do {
		errno = 0;
		ret = nanosleep(&tval, &tval);
	} while (ret < 0 && errno == EINTR && (tval.tv_sec > 0 || tval.tv_nsec > 0));
#else
	unsigned int tdiff=0;
	struct timeval tval,t1,t2;  
	fd_set fds;

	GetTimeOfDay(&t1);
	t2 = t1;
  
	while (tdiff < t) {
		tval.tv_sec = (t-tdiff)/1000;
		tval.tv_usec = 1000*((t-tdiff)%1000);

		/* Never wait for more than 1 sec. */
		if (tval.tv_sec > 1) {
			tval.tv_sec = 1; 
			tval.tv_usec = 0;
		}

		FD_ZERO(&fds);
		errno = 0;
		sys_select_intr(0,&fds,NULL,NULL,&tval);

		GetTimeOfDay(&t2);
		if (t2.tv_sec < t1.tv_sec) {
			/* Someone adjusted time... */
			t1 = t2;
		}

		tdiff = TvalDiff(&t1,&t2);
	}
#endif
}

/****************************************************************************
 Become a daemon, discarding the controlling terminal.
****************************************************************************/

void become_daemon(bool Fork, bool no_process_group)
{
	if (Fork) {
		if (sys_fork()) {
			_exit(0);
		}
	}

  /* detach from the terminal */
#ifdef HAVE_SETSID
	if (!no_process_group) setsid();
#elif defined(TIOCNOTTY)
	if (!no_process_group) {
		int i = sys_open("/dev/tty", O_RDWR, 0);
		if (i != -1) {
			ioctl(i, (int) TIOCNOTTY, (char *)0);      
			close(i);
		}
	}
#endif /* HAVE_SETSID */

	/* Close fd's 0,1,2. Needed if started by rsh */
	close_low_fds(False);  /* Don't close stderr, let the debug system
				  attach it to the logfile */
}

bool reinit_after_fork(struct messaging_context *msg_ctx,
		       struct event_context *ev_ctx,
		       bool parent_longlived)
{
	NTSTATUS status;

	/* Reset the state of the random
	 * number generation system, so
	 * children do not get the same random
	 * numbers as each other */
	set_need_random_reseed();

	/* tdb needs special fork handling */
	if (tdb_reopen_all(parent_longlived ? 1 : 0) == -1) {
		DEBUG(0,("tdb_reopen_all failed.\n"));
		return false;
	}

	if (ev_ctx) {
		event_context_reinit(ev_ctx);
	}

	if (msg_ctx) {
		/*
		 * For clustering, we need to re-init our ctdbd connection after the
		 * fork
		 */
		status = messaging_reinit(msg_ctx);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0,("messaging_reinit() failed: %s\n",
				 nt_errstr(status)));
			return false;
		}
	}

	return true;
}

/****************************************************************************
 Put up a yes/no prompt.
****************************************************************************/

bool yesno(const char *p)
{
	char ans[20];
	printf("%s",p);

	if (!fgets(ans,sizeof(ans)-1,stdin))
		return(False);

	if (*ans == 'y' || *ans == 'Y')
		return(True);

	return(False);
}

#if defined(PARANOID_MALLOC_CHECKER)

/****************************************************************************
 Internal malloc wrapper. Externally visible.
****************************************************************************/

void *malloc_(size_t size)
{
	if (size == 0) {
		return NULL;
	}
#undef malloc
	return malloc(size);
#define malloc(s) __ERROR_DONT_USE_MALLOC_DIRECTLY
}

/****************************************************************************
 Internal calloc wrapper. Not externally visible.
****************************************************************************/

static void *calloc_(size_t count, size_t size)
{
	if (size == 0 || count == 0) {
		return NULL;
	}
#undef calloc
	return calloc(count, size);
#define calloc(n,s) __ERROR_DONT_USE_CALLOC_DIRECTLY
}

/****************************************************************************
 Internal realloc wrapper. Not externally visible.
****************************************************************************/

static void *realloc_(void *ptr, size_t size)
{
#undef realloc
	return realloc(ptr, size);
#define realloc(p,s) __ERROR_DONT_USE_RELLOC_DIRECTLY
}

#endif /* PARANOID_MALLOC_CHECKER */

/****************************************************************************
 Type-safe malloc.
****************************************************************************/

void *malloc_array(size_t el_size, unsigned int count)
{
	if (count >= MAX_ALLOC_SIZE/el_size) {
		return NULL;
	}

	if (el_size == 0 || count == 0) {
		return NULL;
	}
#if defined(PARANOID_MALLOC_CHECKER)
	return malloc_(el_size*count);
#else
	return malloc(el_size*count);
#endif
}

/****************************************************************************
 Type-safe memalign
****************************************************************************/

void *memalign_array(size_t el_size, size_t align, unsigned int count)
{
	if (count >= MAX_ALLOC_SIZE/el_size) {
		return NULL;
	}

	return sys_memalign(align, el_size*count);
}

/****************************************************************************
 Type-safe calloc.
****************************************************************************/

void *calloc_array(size_t size, size_t nmemb)
{
	if (nmemb >= MAX_ALLOC_SIZE/size) {
		return NULL;
	}
	if (size == 0 || nmemb == 0) {
		return NULL;
	}
#if defined(PARANOID_MALLOC_CHECKER)
	return calloc_(nmemb, size);
#else
	return calloc(nmemb, size);
#endif
}

/****************************************************************************
 Expand a pointer to be a particular size.
 Note that this version of Realloc has an extra parameter that decides
 whether to free the passed in storage on allocation failure or if the
 new size is zero.

 This is designed for use in the typical idiom of :

 p = SMB_REALLOC(p, size)
 if (!p) {
    return error;
 }

 and not to have to keep track of the old 'p' contents to free later, nor
 to worry if the size parameter was zero. In the case where NULL is returned
 we guarentee that p has been freed.

 If free later semantics are desired, then pass 'free_old_on_error' as False which
 guarentees that the old contents are not freed on error, even if size == 0. To use
 this idiom use :

 tmp = SMB_REALLOC_KEEP_OLD_ON_ERROR(p, size);
 if (!tmp) {
    SAFE_FREE(p);
    return error;
 } else {
    p = tmp;
 }

 Changes were instigated by Coverity error checking. JRA.
****************************************************************************/

void *Realloc(void *p, size_t size, bool free_old_on_error)
{
	void *ret=NULL;

	if (size == 0) {
		if (free_old_on_error) {
			SAFE_FREE(p);
		}
		DEBUG(2,("Realloc asked for 0 bytes\n"));
		return NULL;
	}

#if defined(PARANOID_MALLOC_CHECKER)
	if (!p) {
		ret = (void *)malloc_(size);
	} else {
		ret = (void *)realloc_(p,size);
	}
#else
	if (!p) {
		ret = (void *)malloc(size);
	} else {
		ret = (void *)realloc(p,size);
	}
#endif

	if (!ret) {
		if (free_old_on_error && p) {
			SAFE_FREE(p);
		}
		DEBUG(0,("Memory allocation error: failed to expand to %d bytes\n",(int)size));
	}

	return(ret);
}

/****************************************************************************
 Type-safe realloc.
****************************************************************************/

void *realloc_array(void *p, size_t el_size, unsigned int count, bool free_old_on_error)
{
	if (count >= MAX_ALLOC_SIZE/el_size) {
		if (free_old_on_error) {
			SAFE_FREE(p);
		}
		return NULL;
	}
	return Realloc(p, el_size*count, free_old_on_error);
}

/****************************************************************************
 (Hopefully) efficient array append.
****************************************************************************/

void add_to_large_array(TALLOC_CTX *mem_ctx, size_t element_size,
			void *element, void *_array, uint32 *num_elements,
			ssize_t *array_size)
{
	void **array = (void **)_array;

	if (*array_size < 0) {
		return;
	}

	if (*array == NULL) {
		if (*array_size == 0) {
			*array_size = 128;
		}

		if (*array_size >= MAX_ALLOC_SIZE/element_size) {
			goto error;
		}

		*array = TALLOC(mem_ctx, element_size * (*array_size));
		if (*array == NULL) {
			goto error;
		}
	}

	if (*num_elements == *array_size) {
		*array_size *= 2;

		if (*array_size >= MAX_ALLOC_SIZE/element_size) {
			goto error;
		}

		*array = TALLOC_REALLOC(mem_ctx, *array,
					element_size * (*array_size));

		if (*array == NULL) {
			goto error;
		}
	}

	memcpy((char *)(*array) + element_size*(*num_elements),
	       element, element_size);
	*num_elements += 1;

	return;

 error:
	*num_elements = 0;
	*array_size = -1;
}

/****************************************************************************
 Free memory, checks for NULL.
 Use directly SAFE_FREE()
 Exists only because we need to pass a function pointer somewhere --SSS
****************************************************************************/

void safe_free(void *p)
{
	SAFE_FREE(p);
}

/****************************************************************************
 Get my own name and IP.
****************************************************************************/

char *get_myname(TALLOC_CTX *ctx)
{
	char *p;
	char hostname[HOST_NAME_MAX];

	*hostname = 0;

	/* get my host name */
	if (gethostname(hostname, sizeof(hostname)) == -1) {
		DEBUG(0,("gethostname failed\n"));
		return False;
	}

	/* Ensure null termination. */
	hostname[sizeof(hostname)-1] = '\0';

	/* split off any parts after an initial . */
	p = strchr_m(hostname,'.');
	if (p) {
		*p = 0;
	}

	return talloc_strdup(ctx, hostname);
}

/****************************************************************************
 Get my own domain name, or "" if we have none.
****************************************************************************/

char *get_mydnsdomname(TALLOC_CTX *ctx)
{
	const char *domname;
	char *p;

	domname = get_mydnsfullname();
	if (!domname) {
		return NULL;
	}

	p = strchr_m(domname, '.');
	if (p) {
		p++;
		return talloc_strdup(ctx, p);
	} else {
		return talloc_strdup(ctx, "");
	}
}

/****************************************************************************
 Interpret a protocol description string, with a default.
****************************************************************************/

int interpret_protocol(const char *str,int def)
{
	if (strequal(str,"NT1"))
		return(PROTOCOL_NT1);
	if (strequal(str,"LANMAN2"))
		return(PROTOCOL_LANMAN2);
	if (strequal(str,"LANMAN1"))
		return(PROTOCOL_LANMAN1);
	if (strequal(str,"CORE"))
		return(PROTOCOL_CORE);
	if (strequal(str,"COREPLUS"))
		return(PROTOCOL_COREPLUS);
	if (strequal(str,"CORE+"))
		return(PROTOCOL_COREPLUS);
  
	DEBUG(0,("Unrecognised protocol level %s\n",str));
  
	return(def);
}


#if (defined(HAVE_NETGROUP) && defined(WITH_AUTOMOUNT))
/******************************************************************
 Remove any mount options such as -rsize=2048,wsize=2048 etc.
 Based on a fix from <Thomas.Hepper@icem.de>.
 Returns a malloc'ed string.
*******************************************************************/

static char *strip_mount_options(TALLOC_CTX *ctx, const char *str)
{
	if (*str == '-') {
		const char *p = str;
		while(*p && !isspace(*p))
			p++;
		while(*p && isspace(*p))
			p++;
		if(*p) {
			return talloc_strdup(ctx, p);
		}
	}
	return NULL;
}

/*******************************************************************
 Patch from jkf@soton.ac.uk
 Split Luke's automount_server into YP lookup and string splitter
 so can easily implement automount_path().
 Returns a malloc'ed string.
*******************************************************************/

#ifdef WITH_NISPLUS_HOME
char *automount_lookup(TALLOC_CTX *ctx, const char *user_name)
{
	char *value = NULL;

	char *nis_map = (char *)lp_nis_home_map_name();

	char buffer[NIS_MAXATTRVAL + 1];
	nis_result *result;
	nis_object *object;
	entry_obj  *entry;

	snprintf(buffer, sizeof(buffer), "[key=%s],%s", user_name, nis_map);
	DEBUG(5, ("NIS+ querystring: %s\n", buffer));

	if (result = nis_list(buffer, FOLLOW_PATH|EXPAND_NAME|HARD_LOOKUP, NULL, NULL)) {
		if (result->status != NIS_SUCCESS) {
			DEBUG(3, ("NIS+ query failed: %s\n", nis_sperrno(result->status)));
		} else {
			object = result->objects.objects_val;
			if (object->zo_data.zo_type == ENTRY_OBJ) {
				entry = &object->zo_data.objdata_u.en_data;
				DEBUG(5, ("NIS+ entry type: %s\n", entry->en_type));
				DEBUG(3, ("NIS+ result: %s\n", entry->en_cols.en_cols_val[1].ec_value.ec_value_val));

				value = talloc_strdup(ctx,
						entry->en_cols.en_cols_val[1].ec_value.ec_value_val);
				if (!value) {
					nis_freeresult(result);
					return NULL;
				}
				value = talloc_string_sub(ctx,
						value,
						"&",
						user_name);
			}
		}
	}
	nis_freeresult(result);

	if (value) {
		value = strip_mount_options(ctx, value);
		DEBUG(4, ("NIS+ Lookup: %s resulted in %s\n",
					user_name, value));
	}
	return value;
}
#else /* WITH_NISPLUS_HOME */

char *automount_lookup(TALLOC_CTX *ctx, const char *user_name)
{
	char *value = NULL;

	int nis_error;        /* returned by yp all functions */
	char *nis_result;     /* yp_match inits this */
	int nis_result_len;  /* and set this */
	char *nis_domain;     /* yp_get_default_domain inits this */
	char *nis_map = (char *)lp_nis_home_map_name();

	if ((nis_error = yp_get_default_domain(&nis_domain)) != 0) {
		DEBUG(3, ("YP Error: %s\n", yperr_string(nis_error)));
		return NULL;
	}

	DEBUG(5, ("NIS Domain: %s\n", nis_domain));

	if ((nis_error = yp_match(nis_domain, nis_map, user_name,
					strlen(user_name), &nis_result,
					&nis_result_len)) == 0) {
		value = talloc_strdup(ctx, nis_result);
		if (!value) {
			return NULL;
		}
		value = strip_mount_options(ctx, value);
	} else if(nis_error == YPERR_KEY) {
		DEBUG(3, ("YP Key not found:  while looking up \"%s\" in map \"%s\"\n", 
				user_name, nis_map));
		DEBUG(3, ("using defaults for server and home directory\n"));
	} else {
		DEBUG(3, ("YP Error: \"%s\" while looking up \"%s\" in map \"%s\"\n", 
				yperr_string(nis_error), user_name, nis_map));
	}

	if (value) {
		DEBUG(4, ("YP Lookup: %s resulted in %s\n", user_name, value));
	}
	return value;
}
#endif /* WITH_NISPLUS_HOME */
#endif

/****************************************************************************
 Check if a process exists. Does this work on all unixes?
****************************************************************************/

bool process_exists(const struct server_id pid)
{
	if (procid_is_me(&pid)) {
		return True;
	}

	if (procid_is_local(&pid)) {
		return (kill(pid.pid,0) == 0 || errno != ESRCH);
	}

#ifdef CLUSTER_SUPPORT
	return ctdbd_process_exists(messaging_ctdbd_connection(), pid.vnn,
				    pid.pid);
#else
	return False;
#endif
}

bool process_exists_by_pid(pid_t pid)
{
	/* Doing kill with a non-positive pid causes messages to be
	 * sent to places we don't want. */
	SMB_ASSERT(pid > 0);
	return(kill(pid,0) == 0 || errno != ESRCH);
}

/*******************************************************************
 Convert a uid into a user name.
********************************************************************/

const char *uidtoname(uid_t uid)
{
	TALLOC_CTX *ctx = talloc_tos();
	char *name = NULL;
	struct passwd *pass = NULL;

	pass = getpwuid_alloc(ctx,uid);
	if (pass) {
		name = talloc_strdup(ctx,pass->pw_name);
		TALLOC_FREE(pass);
	} else {
		name = talloc_asprintf(ctx,
				"%ld",
				(long int)uid);
	}
	return name;
}

/*******************************************************************
 Convert a gid into a group name.
********************************************************************/

char *gidtoname(gid_t gid)
{
	struct group *grp;

	grp = getgrgid(gid);
	if (grp) {
		return talloc_strdup(talloc_tos(), grp->gr_name);
	}
	else {
		return talloc_asprintf(talloc_tos(),
					"%d",
					(int)gid);
	}
}

/*******************************************************************
 Convert a user name into a uid.
********************************************************************/

uid_t nametouid(const char *name)
{
	struct passwd *pass;
	char *p;
	uid_t u;

	pass = getpwnam_alloc(NULL, name);
	if (pass) {
		u = pass->pw_uid;
		TALLOC_FREE(pass);
		return u;
	}

	u = (uid_t)strtol(name, &p, 0);
	if ((p != name) && (*p == '\0'))
		return u;

	return (uid_t)-1;
}

/*******************************************************************
 Convert a name to a gid_t if possible. Return -1 if not a group. 
********************************************************************/

gid_t nametogid(const char *name)
{
	struct group *grp;
	char *p;
	gid_t g;

	g = (gid_t)strtol(name, &p, 0);
	if ((p != name) && (*p == '\0'))
		return g;

	grp = sys_getgrnam(name);
	if (grp)
		return(grp->gr_gid);
	return (gid_t)-1;
}

/*******************************************************************
 Something really nasty happened - panic !
********************************************************************/

void smb_panic(const char *const why)
{
	char *cmd;
	int result;

#ifdef DEVELOPER
	{

		if (global_clobber_region_function) {
			DEBUG(0,("smb_panic: clobber_region() last called from [%s(%u)]\n",
					 global_clobber_region_function,
					 global_clobber_region_line));
		} 
	}
#endif

	DEBUG(0,("PANIC (pid %llu): %s\n",
		    (unsigned long long)sys_getpid(), why));
	log_stack_trace();

	cmd = lp_panic_action();
	if (cmd && *cmd) {
		DEBUG(0, ("smb_panic(): calling panic action [%s]\n", cmd));
		result = system(cmd);

		if (result == -1)
			DEBUG(0, ("smb_panic(): fork failed in panic action: %s\n",
					  strerror(errno)));
		else
			DEBUG(0, ("smb_panic(): action returned status %d\n",
					  WEXITSTATUS(result)));
	}

	dump_core();
}

/*******************************************************************
 Print a backtrace of the stack to the debug log. This function
 DELIBERATELY LEAKS MEMORY. The expectation is that you should
 exit shortly after calling it.
********************************************************************/

#ifdef HAVE_LIBUNWIND_H
#include <libunwind.h>
#endif

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#ifdef HAVE_LIBEXC_H
#include <libexc.h>
#endif

void log_stack_trace(void)
{
#ifdef HAVE_LIBUNWIND
	/* Try to use libunwind before any other technique since on ia64
	 * libunwind correctly walks the stack in more circumstances than
	 * backtrace.
	 */ 
	unw_cursor_t cursor;
	unw_context_t uc;
	unsigned i = 0;

	char procname[256];
	unw_word_t ip, sp, off;

	procname[sizeof(procname) - 1] = '\0';

	if (unw_getcontext(&uc) != 0) {
		goto libunwind_failed;
	}

	if (unw_init_local(&cursor, &uc) != 0) {
		goto libunwind_failed;
	}

	DEBUG(0, ("BACKTRACE:\n"));

	do {
	    ip = sp = 0;
	    unw_get_reg(&cursor, UNW_REG_IP, &ip);
	    unw_get_reg(&cursor, UNW_REG_SP, &sp);

	    switch (unw_get_proc_name(&cursor,
			procname, sizeof(procname) - 1, &off) ) {
	    case 0:
		    /* Name found. */
	    case -UNW_ENOMEM:
		    /* Name truncated. */
		    DEBUGADD(0, (" #%u %s + %#llx [ip=%#llx] [sp=%#llx]\n",
			    i, procname, (long long)off,
			    (long long)ip, (long long) sp));
		    break;
	    default:
	    /* case -UNW_ENOINFO: */
	    /* case -UNW_EUNSPEC: */
		    /* No symbol name found. */
		    DEBUGADD(0, (" #%u %s [ip=%#llx] [sp=%#llx]\n",
			    i, "<unknown symbol>",
			    (long long)ip, (long long) sp));
	    }
	    ++i;
	} while (unw_step(&cursor) > 0);

	return;

libunwind_failed:
	DEBUG(0, ("unable to produce a stack trace with libunwind\n"));

#elif HAVE_BACKTRACE_SYMBOLS
	void *backtrace_stack[BACKTRACE_STACK_SIZE];
	size_t backtrace_size;
	char **backtrace_strings;

	/* get the backtrace (stack frames) */
	backtrace_size = backtrace(backtrace_stack,BACKTRACE_STACK_SIZE);
	backtrace_strings = backtrace_symbols(backtrace_stack, backtrace_size);

	DEBUG(0, ("BACKTRACE: %lu stack frames:\n", 
		  (unsigned long)backtrace_size));
	
	if (backtrace_strings) {
		int i;

		for (i = 0; i < backtrace_size; i++)
			DEBUGADD(0, (" #%u %s\n", i, backtrace_strings[i]));

		/* Leak the backtrace_strings, rather than risk what free() might do */
	}

#elif HAVE_LIBEXC

	/* The IRIX libexc library provides an API for unwinding the stack. See
	 * libexc(3) for details. Apparantly trace_back_stack leaks memory, but
	 * since we are about to abort anyway, it hardly matters.
	 */

#define NAMESIZE 32 /* Arbitrary */

	__uint64_t	addrs[BACKTRACE_STACK_SIZE];
	char *      	names[BACKTRACE_STACK_SIZE];
	char		namebuf[BACKTRACE_STACK_SIZE * NAMESIZE];

	int		i;
	int		levels;

	ZERO_ARRAY(addrs);
	ZERO_ARRAY(names);
	ZERO_ARRAY(namebuf);

	/* We need to be root so we can open our /proc entry to walk
	 * our stack. It also helps when we want to dump core.
	 */
	become_root();

	for (i = 0; i < BACKTRACE_STACK_SIZE; i++) {
		names[i] = namebuf + (i * NAMESIZE);
	}

	levels = trace_back_stack(0, addrs, names,
			BACKTRACE_STACK_SIZE, NAMESIZE - 1);

	DEBUG(0, ("BACKTRACE: %d stack frames:\n", levels));
	for (i = 0; i < levels; i++) {
		DEBUGADD(0, (" #%d 0x%llx %s\n", i, addrs[i], names[i]));
	}
#undef NAMESIZE

#else
	DEBUG(0, ("unable to produce a stack trace on this platform\n"));
#endif
}

/*******************************************************************
  A readdir wrapper which just returns the file name.
 ********************************************************************/

const char *readdirname(SMB_STRUCT_DIR *p)
{
	SMB_STRUCT_DIRENT *ptr;
	char *dname;

	if (!p)
		return(NULL);
  
	ptr = (SMB_STRUCT_DIRENT *)sys_readdir(p);
	if (!ptr)
		return(NULL);

	dname = ptr->d_name;

#ifdef NEXT2
	if (telldir(p) < 0)
		return(NULL);
#endif

#ifdef HAVE_BROKEN_READDIR_NAME
	/* using /usr/ucb/cc is BAD */
	dname = dname - 2;
#endif

	return talloc_strdup(talloc_tos(), dname);
}

/*******************************************************************
 Utility function used to decide if the last component 
 of a path matches a (possibly wildcarded) entry in a namelist.
********************************************************************/

bool is_in_path(const char *name, name_compare_entry *namelist, bool case_sensitive)
{
	const char *last_component;

	/* if we have no list it's obviously not in the path */
	if((namelist == NULL ) || ((namelist != NULL) && (namelist[0].name == NULL))) {
		return False;
	}

	DEBUG(8, ("is_in_path: %s\n", name));

	/* Get the last component of the unix name. */
	last_component = strrchr_m(name, '/');
	if (!last_component) {
		last_component = name;
	} else {
		last_component++; /* Go past '/' */
	}

	for(; namelist->name != NULL; namelist++) {
		if(namelist->is_wild) {
			if (mask_match(last_component, namelist->name, case_sensitive)) {
				DEBUG(8,("is_in_path: mask match succeeded\n"));
				return True;
			}
		} else {
			if((case_sensitive && (strcmp(last_component, namelist->name) == 0))||
						(!case_sensitive && (StrCaseCmp(last_component, namelist->name) == 0))) {
				DEBUG(8,("is_in_path: match succeeded\n"));
				return True;
			}
		}
	}
	DEBUG(8,("is_in_path: match not found\n"));
	return False;
}

/*******************************************************************
 Strip a '/' separated list into an array of 
 name_compare_enties structures suitable for 
 passing to is_in_path(). We do this for
 speed so we can pre-parse all the names in the list 
 and don't do it for each call to is_in_path().
 namelist is modified here and is assumed to be 
 a copy owned by the caller.
 We also check if the entry contains a wildcard to
 remove a potentially expensive call to mask_match
 if possible.
********************************************************************/
 
void set_namearray(name_compare_entry **ppname_array, char *namelist)
{
	char *name_end;
	char *nameptr = namelist;
	int num_entries = 0;
	int i;

	(*ppname_array) = NULL;

	if((nameptr == NULL ) || ((nameptr != NULL) && (*nameptr == '\0'))) 
		return;

	/* We need to make two passes over the string. The
		first to count the number of elements, the second
		to split it.
	*/

	while(*nameptr) {
		if ( *nameptr == '/' ) {
			/* cope with multiple (useless) /s) */
			nameptr++;
			continue;
		}
		/* find the next / */
		name_end = strchr_m(nameptr, '/');

		/* oops - the last check for a / didn't find one. */
		if (name_end == NULL)
			break;

		/* next segment please */
		nameptr = name_end + 1;
		num_entries++;
	}

	if(num_entries == 0)
		return;

	if(( (*ppname_array) = SMB_MALLOC_ARRAY(name_compare_entry, num_entries + 1)) == NULL) {
		DEBUG(0,("set_namearray: malloc fail\n"));
		return;
	}

	/* Now copy out the names */
	nameptr = namelist;
	i = 0;
	while(*nameptr) {
		if ( *nameptr == '/' ) {
			/* cope with multiple (useless) /s) */
			nameptr++;
			continue;
		}
		/* find the next / */
		if ((name_end = strchr_m(nameptr, '/')) != NULL)
			*name_end = 0;

		/* oops - the last check for a / didn't find one. */
		if(name_end == NULL) 
			break;

		(*ppname_array)[i].is_wild = ms_has_wild(nameptr);
		if(((*ppname_array)[i].name = SMB_STRDUP(nameptr)) == NULL) {
			DEBUG(0,("set_namearray: malloc fail (1)\n"));
			return;
		}

		/* next segment please */
		nameptr = name_end + 1;
		i++;
	}
  
	(*ppname_array)[i].name = NULL;

	return;
}

/****************************************************************************
 Routine to free a namearray.
****************************************************************************/

void free_namearray(name_compare_entry *name_array)
{
	int i;

	if(name_array == NULL)
		return;

	for(i=0; name_array[i].name!=NULL; i++)
		SAFE_FREE(name_array[i].name);
	SAFE_FREE(name_array);
}

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_LOCKING

/****************************************************************************
 Simple routine to do POSIX file locking. Cruft in NFS and 64->32 bit mapping
 is dealt with in posix.c
 Returns True if the lock was granted, False otherwise.
****************************************************************************/

bool fcntl_lock(int fd, int op, SMB_OFF_T offset, SMB_OFF_T count, int type)
{
	SMB_STRUCT_FLOCK lock;
	int ret;

	DEBUG(8,("fcntl_lock fd=%d op=%d offset=%.0f count=%.0f type=%d\n",
		fd,op,(double)offset,(double)count,type));

	lock.l_type = type;
	lock.l_whence = SEEK_SET;
	lock.l_start = offset;
	lock.l_len = count;
	lock.l_pid = 0;

	ret = sys_fcntl_ptr(fd,op,&lock);

	if (ret == -1) {
		int sav = errno;
		DEBUG(3,("fcntl_lock: lock failed at offset %.0f count %.0f op %d type %d (%s)\n",
			(double)offset,(double)count,op,type,strerror(errno)));
		errno = sav;
		return False;
	}

	/* everything went OK */
	DEBUG(8,("fcntl_lock: Lock call successful\n"));

	return True;
}

/****************************************************************************
 Simple routine to query existing file locks. Cruft in NFS and 64->32 bit mapping
 is dealt with in posix.c
 Returns True if we have information regarding this lock region (and returns
 F_UNLCK in *ptype if the region is unlocked). False if the call failed.
****************************************************************************/

bool fcntl_getlock(int fd, SMB_OFF_T *poffset, SMB_OFF_T *pcount, int *ptype, pid_t *ppid)
{
	SMB_STRUCT_FLOCK lock;
	int ret;

	DEBUG(8,("fcntl_getlock fd=%d offset=%.0f count=%.0f type=%d\n",
		    fd,(double)*poffset,(double)*pcount,*ptype));

	lock.l_type = *ptype;
	lock.l_whence = SEEK_SET;
	lock.l_start = *poffset;
	lock.l_len = *pcount;
	lock.l_pid = 0;

	ret = sys_fcntl_ptr(fd,SMB_F_GETLK,&lock);

	if (ret == -1) {
		int sav = errno;
		DEBUG(3,("fcntl_getlock: lock request failed at offset %.0f count %.0f type %d (%s)\n",
			(double)*poffset,(double)*pcount,*ptype,strerror(errno)));
		errno = sav;
		return False;
	}

	*ptype = lock.l_type;
	*poffset = lock.l_start;
	*pcount = lock.l_len;
	*ppid = lock.l_pid;
	
	DEBUG(3,("fcntl_getlock: fd %d is returned info %d pid %u\n",
			fd, (int)lock.l_type, (unsigned int)lock.l_pid));
	return True;
}

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_ALL

/*******************************************************************
 Is the name specified one of my netbios names.
 Returns true if it is equal, false otherwise.
********************************************************************/

bool is_myname(const char *s)
{
	int n;
	bool ret = False;

	for (n=0; my_netbios_names(n); n++) {
		if (strequal(my_netbios_names(n), s)) {
			ret=True;
			break;
		}
	}
	DEBUG(8, ("is_myname(\"%s\") returns %d\n", s, ret));
	return(ret);
}

/*******************************************************************
 Is the name specified our workgroup/domain.
 Returns true if it is equal, false otherwise.
********************************************************************/

bool is_myworkgroup(const char *s)
{
	bool ret = False;

	if (strequal(s, lp_workgroup())) {
		ret=True;
	}

	DEBUG(8, ("is_myworkgroup(\"%s\") returns %d\n", s, ret));
	return(ret);
}

/*******************************************************************
 we distinguish between 2K and XP by the "Native Lan Manager" string
   WinXP => "Windows 2002 5.1"
   WinXP 64bit => "Windows XP 5.2"
   Win2k => "Windows 2000 5.0"
   NT4   => "Windows NT 4.0"
   Win9x => "Windows 4.0"
 Windows 2003 doesn't set the native lan manager string but
 they do set the domain to "Windows 2003 5.2" (probably a bug).
********************************************************************/

void ra_lanman_string( const char *native_lanman )
{
	if ( strcmp( native_lanman, "Windows 2002 5.1" ) == 0 )
		set_remote_arch( RA_WINXP );
	else if ( strcmp( native_lanman, "Windows XP 5.2" ) == 0 )
		set_remote_arch( RA_WINXP64 );
	else if ( strcmp( native_lanman, "Windows Server 2003 5.2" ) == 0 )
		set_remote_arch( RA_WIN2K3 );
}

static const char *remote_arch_str;

const char *get_remote_arch_str(void)
{
	if (!remote_arch_str) {
		return "UNKNOWN";
	}
	return remote_arch_str;
}

/*******************************************************************
 Set the horrid remote_arch string based on an enum.
********************************************************************/

void set_remote_arch(enum remote_arch_types type)
{
	ra_type = type;
	switch( type ) {
	case RA_WFWG:
		remote_arch_str = "WfWg";
		break;
	case RA_OS2:
		remote_arch_str = "OS2";
		break;
	case RA_WIN95:
		remote_arch_str = "Win95";
		break;
	case RA_WINNT:
		remote_arch_str = "WinNT";
		break;
	case RA_WIN2K:
		remote_arch_str = "Win2K";
		break;
	case RA_WINXP:
		remote_arch_str = "WinXP";
		break;
	case RA_WINXP64:
		remote_arch_str = "WinXP64";
		break;
	case RA_WIN2K3:
		remote_arch_str = "Win2K3";
		break;
	case RA_VISTA:
		remote_arch_str = "Vista";
		break;
	case RA_SAMBA:
		remote_arch_str = "Samba";
		break;
	case RA_CIFSFS:
		remote_arch_str = "CIFSFS";
		break;
	default:
		ra_type = RA_UNKNOWN;
		remote_arch_str = "UNKNOWN";
		break;
	}

	DEBUG(10,("set_remote_arch: Client arch is \'%s\'\n",
				remote_arch_str));
}

/*******************************************************************
 Get the remote_arch type.
********************************************************************/

enum remote_arch_types get_remote_arch(void)
{
	return ra_type;
}

void print_asc(int level, const unsigned char *buf,int len)
{
	int i;
	for (i=0;i<len;i++)
		DEBUG(level,("%c", isprint(buf[i])?buf[i]:'.'));
}

void dump_data(int level, const unsigned char *buf1,int len)
{
	const unsigned char *buf = (const unsigned char *)buf1;
	int i=0;
	if (len<=0) return;

	if (!DEBUGLVL(level)) return;
	
	DEBUGADD(level,("[%03X] ",i));
	for (i=0;i<len;) {
		DEBUGADD(level,("%02X ",(int)buf[i]));
		i++;
		if (i%8 == 0) DEBUGADD(level,(" "));
		if (i%16 == 0) {      
			print_asc(level,&buf[i-16],8); DEBUGADD(level,(" "));
			print_asc(level,&buf[i-8],8); DEBUGADD(level,("\n"));
			if (i<len) DEBUGADD(level,("[%03X] ",i));
		}
	}
	if (i%16) {
		int n;
		n = 16 - (i%16);
		DEBUGADD(level,(" "));
		if (n>8) DEBUGADD(level,(" "));
		while (n--) DEBUGADD(level,("   "));
		n = MIN(8,i%16);
		print_asc(level,&buf[i-(i%16)],n); DEBUGADD(level,( " " ));
		n = (i%16) - n;
		if (n>0) print_asc(level,&buf[i-n],n);
		DEBUGADD(level,("\n"));
	}
}

void dump_data_pw(const char *msg, const uchar * data, size_t len)
{
#ifdef DEBUG_PASSWORD
	DEBUG(11, ("%s", msg));
	if (data != NULL && len > 0)
	{
		dump_data(11, data, len);
	}
#endif
}

const char *tab_depth(int level, int depth)
{
	if( CHECK_DEBUGLVL(level) ) {
		dbgtext("%*s", depth*4, "");
	}
	return "";
}

/*****************************************************************************
 Provide a checksum on a string

 Input:  s - the null-terminated character string for which the checksum
             will be calculated.

  Output: The checksum value calculated for s.
*****************************************************************************/

int str_checksum(const char *s)
{
	int res = 0;
	int c;
	int i=0;

	while(*s) {
		c = *s;
		res ^= (c << (i % 15)) ^ (c >> (15-(i%15)));
		s++;
		i++;
	}
	return(res);
}

/*****************************************************************
 Zero a memory area then free it. Used to catch bugs faster.
*****************************************************************/  

void zero_free(void *p, size_t size)
{
	memset(p, 0, size);
	SAFE_FREE(p);
}

/*****************************************************************
 Set our open file limit to a requested max and return the limit.
*****************************************************************/  

int set_maxfiles(int requested_max)
{
#if (defined(HAVE_GETRLIMIT) && defined(RLIMIT_NOFILE))
	struct rlimit rlp;
	int saved_current_limit;

	if(getrlimit(RLIMIT_NOFILE, &rlp)) {
		DEBUG(0,("set_maxfiles: getrlimit (1) for RLIMIT_NOFILE failed with error %s\n",
			strerror(errno) ));
		/* just guess... */
		return requested_max;
	}

	/* 
	 * Set the fd limit to be real_max_open_files + MAX_OPEN_FUDGEFACTOR to
	 * account for the extra fd we need 
	 * as well as the log files and standard
	 * handles etc. Save the limit we want to set in case
	 * we are running on an OS that doesn't support this limit (AIX)
	 * which always returns RLIM_INFINITY for rlp.rlim_max.
	 */

	/* Try raising the hard (max) limit to the requested amount. */

#if defined(RLIM_INFINITY)
	if (rlp.rlim_max != RLIM_INFINITY) {
		int orig_max = rlp.rlim_max;

		if ( rlp.rlim_max < requested_max )
			rlp.rlim_max = requested_max;

		/* This failing is not an error - many systems (Linux) don't
			support our default request of 10,000 open files. JRA. */

		if(setrlimit(RLIMIT_NOFILE, &rlp)) {
			DEBUG(3,("set_maxfiles: setrlimit for RLIMIT_NOFILE for %d max files failed with error %s\n", 
				(int)rlp.rlim_max, strerror(errno) ));

			/* Set failed - restore original value from get. */
			rlp.rlim_max = orig_max;
		}
	}
#endif

	/* Now try setting the soft (current) limit. */

	saved_current_limit = rlp.rlim_cur = MIN(requested_max,rlp.rlim_max);

	if(setrlimit(RLIMIT_NOFILE, &rlp)) {
		DEBUG(0,("set_maxfiles: setrlimit for RLIMIT_NOFILE for %d files failed with error %s\n", 
			(int)rlp.rlim_cur, strerror(errno) ));
		/* just guess... */
		return saved_current_limit;
	}

	if(getrlimit(RLIMIT_NOFILE, &rlp)) {
		DEBUG(0,("set_maxfiles: getrlimit (2) for RLIMIT_NOFILE failed with error %s\n",
			strerror(errno) ));
		/* just guess... */
		return saved_current_limit;
    }

#if defined(RLIM_INFINITY)
	if(rlp.rlim_cur == RLIM_INFINITY)
		return saved_current_limit;
#endif

	if((int)rlp.rlim_cur > saved_current_limit)
		return saved_current_limit;

	return rlp.rlim_cur;
#else /* !defined(HAVE_GETRLIMIT) || !defined(RLIMIT_NOFILE) */
	/*
	 * No way to know - just guess...
	 */
	return requested_max;
#endif
}

/*****************************************************************
 Possibly replace mkstemp if it is broken.
*****************************************************************/  

int smb_mkstemp(char *name_template)
{
#if HAVE_SECURE_MKSTEMP
	return mkstemp(name_template);
#else
	/* have a reasonable go at emulating it. Hope that
	   the system mktemp() isn't completly hopeless */
	char *p = mktemp(name_template);
	if (!p)
		return -1;
	return open(p, O_CREAT|O_EXCL|O_RDWR, 0600);
#endif
}

/*****************************************************************
 malloc that aborts with smb_panic on fail or zero size.
 *****************************************************************/  

void *smb_xmalloc_array(size_t size, unsigned int count)
{
	void *p;
	if (size == 0) {
		smb_panic("smb_xmalloc_array: called with zero size");
	}
        if (count >= MAX_ALLOC_SIZE/size) {
                smb_panic("smb_xmalloc_array: alloc size too large");
        }
	if ((p = SMB_MALLOC(size*count)) == NULL) {
		DEBUG(0, ("smb_xmalloc_array failed to allocate %lu * %lu bytes\n",
			(unsigned long)size, (unsigned long)count));
		smb_panic("smb_xmalloc_array: malloc failed");
	}
	return p;
}

/**
 Memdup with smb_panic on fail.
**/

void *smb_xmemdup(const void *p, size_t size)
{
	void *p2;
	p2 = SMB_XMALLOC_ARRAY(unsigned char,size);
	memcpy(p2, p, size);
	return p2;
}

/**
 strdup that aborts on malloc fail.
**/

char *smb_xstrdup(const char *s)
{
#if defined(PARANOID_MALLOC_CHECKER)
#ifdef strdup
#undef strdup
#endif
#endif

#ifndef HAVE_STRDUP
#define strdup rep_strdup
#endif

	char *s1 = strdup(s);
#if defined(PARANOID_MALLOC_CHECKER)
#ifdef strdup
#undef strdup
#endif
#define strdup(s) __ERROR_DONT_USE_STRDUP_DIRECTLY
#endif
	if (!s1) {
		smb_panic("smb_xstrdup: malloc failed");
	}
	return s1;

}

/**
 strndup that aborts on malloc fail.
**/

char *smb_xstrndup(const char *s, size_t n)
{
#if defined(PARANOID_MALLOC_CHECKER)
#ifdef strndup
#undef strndup
#endif
#endif

#if (defined(BROKEN_STRNDUP) || !defined(HAVE_STRNDUP))
#undef HAVE_STRNDUP
#define strndup rep_strndup
#endif

	char *s1 = strndup(s, n);
#if defined(PARANOID_MALLOC_CHECKER)
#ifdef strndup
#undef strndup
#endif
#define strndup(s,n) __ERROR_DONT_USE_STRNDUP_DIRECTLY
#endif
	if (!s1) {
		smb_panic("smb_xstrndup: malloc failed");
	}
	return s1;
}

/*
  vasprintf that aborts on malloc fail
*/

 int smb_xvasprintf(char **ptr, const char *format, va_list ap)
{
	int n;
	va_list ap2;

	VA_COPY(ap2, ap);

	n = vasprintf(ptr, format, ap2);
	if (n == -1 || ! *ptr) {
		smb_panic("smb_xvasprintf: out of memory");
	}
	va_end(ap2);
	return n;
}

/*****************************************************************
 Like strdup but for memory.
*****************************************************************/

void *memdup(const void *p, size_t size)
{
	void *p2;
	if (size == 0)
		return NULL;
	p2 = SMB_MALLOC(size);
	if (!p2)
		return NULL;
	memcpy(p2, p, size);
	return p2;
}

/*****************************************************************
 Get local hostname and cache result.
*****************************************************************/

char *myhostname(void)
{
	static char *ret;
	if (ret == NULL) {
		/* This is cached forever so
		 * use NULL talloc ctx. */
		ret = get_myname(NULL);
	}
	return ret;
}

/*****************************************************************
 A useful function for returning a path in the Samba pid directory.
*****************************************************************/

static char *xx_path(const char *name, const char *rootpath)
{
	char *fname = NULL;

	fname = talloc_strdup(talloc_tos(), rootpath);
	if (!fname) {
		return NULL;
	}
	trim_string(fname,"","/");

	if (!directory_exist(fname,NULL)) {
		mkdir(fname,0755);
	}

	return talloc_asprintf(talloc_tos(),
				"%s/%s",
				fname,
				name);
}

/*****************************************************************
 A useful function for returning a path in the Samba lock directory.
*****************************************************************/

char *lock_path(const char *name)
{
	return xx_path(name, lp_lockdir());
}

/*****************************************************************
 A useful function for returning a path in the Samba pid directory.
*****************************************************************/

char *pid_path(const char *name)
{
	return xx_path(name, lp_piddir());
}

/**
 * @brief Returns an absolute path to a file in the Samba lib directory.
 *
 * @param name File to find, relative to LIBDIR.
 *
 * @retval Pointer to a string containing the full path.
 **/

char *lib_path(const char *name)
{
	return talloc_asprintf(talloc_tos(), "%s/%s", get_dyn_LIBDIR(), name);
}

/**
 * @brief Returns an absolute path to a file in the Samba data directory.
 *
 * @param name File to find, relative to CODEPAGEDIR.
 *
 * @retval Pointer to a talloc'ed string containing the full path.
 **/

char *data_path(const char *name)
{
	return talloc_asprintf(talloc_tos(), "%s/%s", get_dyn_CODEPAGEDIR(), name);
}

/*****************************************************************
a useful function for returning a path in the Samba state directory
 *****************************************************************/

char *state_path(const char *name)
{
	return xx_path(name, get_dyn_STATEDIR());
}

/**
 * @brief Returns the platform specific shared library extension.
 *
 * @retval Pointer to a const char * containing the extension.
 **/

const char *shlib_ext(void)
{
	return get_dyn_SHLIBEXT();
}

/*******************************************************************
 Given a filename - get its directory name
 NB: Returned in static storage.  Caveats:
 o  If caller wishes to preserve, they should copy.
********************************************************************/

char *parent_dirname(const char *path)
{
	char *parent;

	if (!parent_dirname_talloc(talloc_tos(), path, &parent, NULL)) {
		return NULL;
	}

	return parent;
}

bool parent_dirname_talloc(TALLOC_CTX *mem_ctx, const char *dir,
			   char **parent, const char **name)
{
	char *p;
	ptrdiff_t len;
 
	p = strrchr_m(dir, '/'); /* Find final '/', if any */

	if (p == NULL) {
		if (!(*parent = talloc_strdup(mem_ctx, "."))) {
			return False;
		}
		if (name) {
			*name = "";
		}
		return True;
	}

	len = p-dir;

	if (!(*parent = TALLOC_ARRAY(mem_ctx, char, len+1))) {
		return False;
	}
	memcpy(*parent, dir, len);
	(*parent)[len] = '\0';

	if (name) {
		*name = p+1;
	}
	return True;
}

/*******************************************************************
 Determine if a pattern contains any Microsoft wildcard characters.
*******************************************************************/

bool ms_has_wild(const char *s)
{
	char c;

	if (lp_posix_pathnames()) {
		/* With posix pathnames no characters are wild. */
		return False;
	}

	while ((c = *s++)) {
		switch (c) {
		case '*':
		case '?':
		case '<':
		case '>':
		case '"':
			return True;
		}
	}
	return False;
}

bool ms_has_wild_w(const smb_ucs2_t *s)
{
	smb_ucs2_t c;
	if (!s) return False;
	while ((c = *s++)) {
		switch (c) {
		case UCS2_CHAR('*'):
		case UCS2_CHAR('?'):
		case UCS2_CHAR('<'):
		case UCS2_CHAR('>'):
		case UCS2_CHAR('"'):
			return True;
		}
	}
	return False;
}

/*******************************************************************
 A wrapper that handles case sensitivity and the special handling
 of the ".." name.
*******************************************************************/

bool mask_match(const char *string, const char *pattern, bool is_case_sensitive)
{
	if (strcmp(string,"..") == 0)
		string = ".";
	if (strcmp(pattern,".") == 0)
		return False;
	
	return ms_fnmatch(pattern, string, Protocol <= PROTOCOL_LANMAN2, is_case_sensitive) == 0;
}

/*******************************************************************
 A wrapper that handles case sensitivity and the special handling
 of the ".." name. Varient that is only called by old search code which requires
 pattern translation.
*******************************************************************/

bool mask_match_search(const char *string, const char *pattern, bool is_case_sensitive)
{
	if (strcmp(string,"..") == 0)
		string = ".";
	if (strcmp(pattern,".") == 0)
		return False;
	
	return ms_fnmatch(pattern, string, True, is_case_sensitive) == 0;
}

/*******************************************************************
 A wrapper that handles a list of patters and calls mask_match()
 on each.  Returns True if any of the patterns match.
*******************************************************************/

bool mask_match_list(const char *string, char **list, int listLen, bool is_case_sensitive)
{
       while (listLen-- > 0) {
               if (mask_match(string, *list++, is_case_sensitive))
                       return True;
       }
       return False;
}

/*********************************************************
 Recursive routine that is called by unix_wild_match.
*********************************************************/

static bool unix_do_match(const char *regexp, const char *str)
{
	const char *p;

	for( p = regexp; *p && *str; ) {

		switch(*p) {
			case '?':
				str++;
				p++;
				break;

			case '*':

				/*
				 * Look for a character matching 
				 * the one after the '*'.
				 */
				p++;
				if(!*p)
					return true; /* Automatic match */
				while(*str) {

					while(*str && (*p != *str))
						str++;

					/*
					 * Patch from weidel@multichart.de. In the case of the regexp
					 * '*XX*' we want to ensure there are at least 2 'X' characters
					 * in the string after the '*' for a match to be made.
					 */

					{
						int matchcount=0;

						/*
						 * Eat all the characters that match, but count how many there were.
						 */

						while(*str && (*p == *str)) {
							str++;
							matchcount++;
						}

						/*
						 * Now check that if the regexp had n identical characters that
						 * matchcount had at least that many matches.
						 */

						while ( *(p+1) && (*(p+1) == *p)) {
							p++;
							matchcount--;
						}

						if ( matchcount <= 0 )
							return false;
					}

					str--; /* We've eaten the match char after the '*' */

					if(unix_do_match(p, str))
						return true;

					if(!*str)
						return false;
					else
						str++;
				}
				return false;

			default:
				if(*str != *p)
					return false;
				str++;
				p++;
				break;
		}
	}

	if(!*p && !*str)
		return true;

	if (!*p && str[0] == '.' && str[1] == 0)
		return true;

	if (!*str && *p == '?') {
		while (*p == '?')
			p++;
		return(!*p);
	}

	if(!*str && (*p == '*' && p[1] == '\0'))
		return true;

	return false;
}

/*******************************************************************
 Simple case insensitive interface to a UNIX wildcard matcher.
 Returns True if match, False if not.
*******************************************************************/

bool unix_wild_match(const char *pattern, const char *string)
{
	TALLOC_CTX *ctx = talloc_stackframe();
	char *p2;
	char *s2;
	char *p;
	bool ret = false;

	p2 = talloc_strdup(ctx,pattern);
	s2 = talloc_strdup(ctx,string);
	if (!p2 || !s2) {
		TALLOC_FREE(ctx);
		return false;
	}
	strlower_m(p2);
	strlower_m(s2);

	/* Remove any *? and ** from the pattern as they are meaningless */
	for(p = p2; *p; p++) {
		while( *p == '*' && (p[1] == '?' ||p[1] == '*')) {
			memmove(&p[1], &p[2], strlen(&p[2])+1);
		}
	}

	if (strequal(p2,"*")) {
		TALLOC_FREE(ctx);
		return true;
	}

	ret = unix_do_match(p2, s2);
	TALLOC_FREE(ctx);
	return ret;
}

/**********************************************************************
 Converts a name to a fully qualified domain name.
 Returns true if lookup succeeded, false if not (then fqdn is set to name)
 Note we deliberately use gethostbyname here, not getaddrinfo as we want
 to examine the h_aliases and I don't know how to do that with getaddrinfo.
***********************************************************************/

bool name_to_fqdn(fstring fqdn, const char *name)
{
	char *full = NULL;
	struct hostent *hp = gethostbyname(name);

	if (!hp || !hp->h_name || !*hp->h_name) {
		DEBUG(10,("name_to_fqdn: lookup for %s failed.\n", name));
		fstrcpy(fqdn, name);
		return false;
	}

	/* Find out if the fqdn is returned as an alias
	 * to cope with /etc/hosts files where the first
	 * name is not the fqdn but the short name */
	if (hp->h_aliases && (! strchr_m(hp->h_name, '.'))) {
		int i;
		for (i = 0; hp->h_aliases[i]; i++) {
			if (strchr_m(hp->h_aliases[i], '.')) {
				full = hp->h_aliases[i];
				break;
			}
		}
	}
	if (full && (StrCaseCmp(full, "localhost.localdomain") == 0)) {
		DEBUG(1, ("WARNING: your /etc/hosts file may be broken!\n"));
		DEBUGADD(1, ("    Specifing the machine hostname for address 127.0.0.1 may lead\n"));
		DEBUGADD(1, ("    to Kerberos authentication problems as localhost.localdomain\n"));
		DEBUGADD(1, ("    may end up being used instead of the real machine FQDN.\n"));
		full = hp->h_name;
	}
	if (!full) {
		full = hp->h_name;
	}

	DEBUG(10,("name_to_fqdn: lookup for %s -> %s.\n", name, full));
	fstrcpy(fqdn, full);
	return true;
}

/**********************************************************************
 Extension to talloc_get_type: Abort on type mismatch
***********************************************************************/

void *talloc_check_name_abort(const void *ptr, const char *name)
{
	void *result;

	result = talloc_check_name(ptr, name);
	if (result != NULL)
		return result;

	DEBUG(0, ("Talloc type mismatch, expected %s, got %s\n",
		  name, talloc_get_name(ptr)));
	smb_panic("talloc type mismatch");
	/* Keep the compiler happy */
	return NULL;
}

uint32 map_share_mode_to_deny_mode(uint32 share_access, uint32 private_options)
{
	switch (share_access & ~FILE_SHARE_DELETE) {
		case FILE_SHARE_NONE:
			return DENY_ALL;
		case FILE_SHARE_READ:
			return DENY_WRITE;
		case FILE_SHARE_WRITE:
			return DENY_READ;
		case FILE_SHARE_READ|FILE_SHARE_WRITE:
			return DENY_NONE;
	}
	if (private_options & NTCREATEX_OPTIONS_PRIVATE_DENY_DOS) {
		return DENY_DOS;
	} else if (private_options & NTCREATEX_OPTIONS_PRIVATE_DENY_FCB) {
		return DENY_FCB;
	}

	return (uint32)-1;
}

pid_t procid_to_pid(const struct server_id *proc)
{
	return proc->pid;
}

static uint32 my_vnn = NONCLUSTER_VNN;

void set_my_vnn(uint32 vnn)
{
	DEBUG(10, ("vnn pid %d = %u\n", (int)sys_getpid(), (unsigned int)vnn));
	my_vnn = vnn;
}

uint32 get_my_vnn(void)
{
	return my_vnn;
}

struct server_id pid_to_procid(pid_t pid)
{
	struct server_id result;
	result.pid = pid;
#ifdef CLUSTER_SUPPORT
	result.vnn = my_vnn;
#endif
	return result;
}

struct server_id procid_self(void)
{
	return pid_to_procid(sys_getpid());
}

struct server_id server_id_self(void)
{
	return procid_self();
}

bool procid_equal(const struct server_id *p1, const struct server_id *p2)
{
	if (p1->pid != p2->pid)
		return False;
#ifdef CLUSTER_SUPPORT
	if (p1->vnn != p2->vnn)
		return False;
#endif
	return True;
}

bool cluster_id_equal(const struct server_id *id1,
		      const struct server_id *id2)
{
	return procid_equal(id1, id2);
}

bool procid_is_me(const struct server_id *pid)
{
	if (pid->pid != sys_getpid())
		return False;
#ifdef CLUSTER_SUPPORT
	if (pid->vnn != my_vnn)
		return False;
#endif
	return True;
}

struct server_id interpret_pid(const char *pid_string)
{
#ifdef CLUSTER_SUPPORT
	unsigned int vnn, pid;
	struct server_id result;
	if (sscanf(pid_string, "%u:%u", &vnn, &pid) == 2) {
		result.vnn = vnn;
		result.pid = pid;
	}
	else if (sscanf(pid_string, "%u", &pid) == 1) {
		result.vnn = NONCLUSTER_VNN;
		result.pid = pid;
	}
	else {
		result.vnn = NONCLUSTER_VNN;
		result.pid = -1;
	}
	return result;
#else
	return pid_to_procid(atoi(pid_string));
#endif
}

char *procid_str(TALLOC_CTX *mem_ctx, const struct server_id *pid)
{
#ifdef CLUSTER_SUPPORT
	if (pid->vnn == NONCLUSTER_VNN) {
		return talloc_asprintf(mem_ctx,
				"%d",
				(int)pid->pid);
	}
	else {
		return talloc_asprintf(mem_ctx,
					"%u:%d",
					(unsigned)pid->vnn,
					(int)pid->pid);
	}
#else
	return talloc_asprintf(mem_ctx,
			"%d",
			(int)pid->pid);
#endif
}

char *procid_str_static(const struct server_id *pid)
{
	return procid_str(talloc_tos(), pid);
}

bool procid_valid(const struct server_id *pid)
{
	return (pid->pid != -1);
}

bool procid_is_local(const struct server_id *pid)
{
#ifdef CLUSTER_SUPPORT
	return pid->vnn == my_vnn;
#else
	return True;
#endif
}

int this_is_smp(void)
{
#if defined(HAVE_SYSCONF)

#if defined(SYSCONF_SC_NPROC_ONLN)
        return (sysconf(_SC_NPROC_ONLN) > 1) ? 1 : 0;
#elif defined(SYSCONF_SC_NPROCESSORS_ONLN)
        return (sysconf(_SC_NPROCESSORS_ONLN) > 1) ? 1 : 0;
#else
	return 0;
#endif

#else
	return 0;
#endif
}

/****************************************************************
 Check if an offset into a buffer is safe.
 If this returns True it's safe to indirect into the byte at
 pointer ptr+off.
****************************************************************/

bool is_offset_safe(const char *buf_base, size_t buf_len, char *ptr, size_t off)
{
	const char *end_base = buf_base + buf_len;
	char *end_ptr = ptr + off;

	if (!buf_base || !ptr) {
		return False;
	}

	if (end_base < buf_base || end_ptr < ptr) {
		return False; /* wrap. */
	}

	if (end_ptr < end_base) {
		return True;
	}
	return False;
}

/****************************************************************
 Return a safe pointer into a buffer, or NULL.
****************************************************************/

char *get_safe_ptr(const char *buf_base, size_t buf_len, char *ptr, size_t off)
{
	return is_offset_safe(buf_base, buf_len, ptr, off) ?
			ptr + off : NULL;
}

/****************************************************************
 Return a safe pointer into a string within a buffer, or NULL.
****************************************************************/

char *get_safe_str_ptr(const char *buf_base, size_t buf_len, char *ptr, size_t off)
{
	if (!is_offset_safe(buf_base, buf_len, ptr, off)) {
		return NULL;
	}
	/* Check if a valid string exists at this offset. */
	if (skip_string(buf_base,buf_len, ptr + off) == NULL) {
		return NULL;
	}
	return ptr + off;
}

/****************************************************************
 Return an SVAL at a pointer, or failval if beyond the end.
****************************************************************/

int get_safe_SVAL(const char *buf_base, size_t buf_len, char *ptr, size_t off, int failval)
{
	/*
	 * Note we use off+1 here, not off+2 as SVAL accesses ptr[0] and ptr[1],
 	 * NOT ptr[2].
 	 */
	if (!is_offset_safe(buf_base, buf_len, ptr, off+1)) {
		return failval;
	}
	return SVAL(ptr,off);
}

/****************************************************************
 Return an IVAL at a pointer, or failval if beyond the end.
****************************************************************/

int get_safe_IVAL(const char *buf_base, size_t buf_len, char *ptr, size_t off, int failval)
{
	/*
	 * Note we use off+3 here, not off+4 as IVAL accesses 
	 * ptr[0] ptr[1] ptr[2] ptr[3] NOT ptr[4].
 	 */
	if (!is_offset_safe(buf_base, buf_len, ptr, off+3)) {
		return failval;
	}
	return IVAL(ptr,off);
}

/****************************************************************
 Split DOM\user into DOM and user. Do not mix with winbind variants of that
 call (they take care of winbind separator and other winbind specific settings).
****************************************************************/

void split_domain_user(TALLOC_CTX *mem_ctx,
		       const char *full_name,
		       char **domain,
		       char **user)
{
	const char *p = NULL;

	p = strchr_m(full_name, '\\');

	if (p != NULL) {
		*domain = talloc_strndup(mem_ctx, full_name,
					 PTR_DIFF(p, full_name));
		*user = talloc_strdup(mem_ctx, p+1);
	} else {
		*domain = talloc_strdup(mem_ctx, "");
		*user = talloc_strdup(mem_ctx, full_name);
	}
}

#if 0

Disable these now we have checked all code paths and ensured
NULL returns on zero request. JRA.

/****************************************************************
 talloc wrapper functions that guarentee a null pointer return
 if size == 0.
****************************************************************/

#ifndef MAX_TALLOC_SIZE
#define MAX_TALLOC_SIZE 0x10000000
#endif

/*
 *    talloc and zero memory.
 *    - returns NULL if size is zero.
 */

void *_talloc_zero_zeronull(const void *ctx, size_t size, const char *name)
{
	void *p;

	if (size == 0) {
		return NULL;
	}

	p = talloc_named_const(ctx, size, name);

	if (p) {
		memset(p, '\0', size);
	}

	return p;
}

/*
 *   memdup with a talloc.
 *   - returns NULL if size is zero.
 */

void *_talloc_memdup_zeronull(const void *t, const void *p, size_t size, const char *name)
{
	void *newp;

	if (size == 0) {
		return NULL;
	}

	newp = talloc_named_const(t, size, name);
	if (newp) {
		memcpy(newp, p, size);
	}

	return newp;
}

/*
 *   alloc an array, checking for integer overflow in the array size.
 *   - returns NULL if count or el_size are zero.
 */

void *_talloc_array_zeronull(const void *ctx, size_t el_size, unsigned count, const char *name)
{
	if (count >= MAX_TALLOC_SIZE/el_size) {
		return NULL;
	}

	if (el_size == 0 || count == 0) {
		return NULL;
	}

	return talloc_named_const(ctx, el_size * count, name);
}

/*
 *   alloc an zero array, checking for integer overflow in the array size
 *   - returns NULL if count or el_size are zero.
 */

void *_talloc_zero_array_zeronull(const void *ctx, size_t el_size, unsigned count, const char *name)
{
	if (count >= MAX_TALLOC_SIZE/el_size) {
		return NULL;
	}

	if (el_size == 0 || count == 0) {
		return NULL;
	}

	return _talloc_zero(ctx, el_size * count, name);
}

/*
 *   Talloc wrapper that returns NULL if size == 0.
 */
void *talloc_zeronull(const void *context, size_t size, const char *name)
{
	if (size == 0) {
		return NULL;
	}
	return talloc_named_const(context, size, name);
}
#endif

/* Split a path name into filename and stream name components. Canonicalise
 * such that an implicit $DATA token is always explicit.
 *
 * The "specification" of this function can be found in the
 * run_local_stream_name() function in torture.c, I've tried those
 * combinations against a W2k3 server.
 */

NTSTATUS split_ntfs_stream_name(TALLOC_CTX *mem_ctx, const char *fname,
				char **pbase, char **pstream)
{
	char *base = NULL;
	char *stream = NULL;
	char *sname; /* stream name */
	const char *stype; /* stream type */

	DEBUG(10, ("split_ntfs_stream_name called for [%s]\n", fname));

	sname = strchr_m(fname, ':');

	if (lp_posix_pathnames() || (sname == NULL)) {
		if (pbase != NULL) {
			base = talloc_strdup(mem_ctx, fname);
			NT_STATUS_HAVE_NO_MEMORY(base);
		}
		goto done;
	}

	if (pbase != NULL) {
		base = talloc_strndup(mem_ctx, fname, PTR_DIFF(sname, fname));
		NT_STATUS_HAVE_NO_MEMORY(base);
	}

	sname += 1;

	stype = strchr_m(sname, ':');

	if (stype == NULL) {
		sname = talloc_strdup(mem_ctx, sname);
		stype = "$DATA";
	}
	else {
		if (StrCaseCmp(stype, ":$DATA") != 0) {
			/*
			 * If there is an explicit stream type, so far we only
			 * allow $DATA. Is there anything else allowed? -- vl
			 */
			DEBUG(10, ("[%s] is an invalid stream type\n", stype));
			TALLOC_FREE(base);
			return NT_STATUS_OBJECT_NAME_INVALID;
		}
		sname = talloc_strndup(mem_ctx, sname, PTR_DIFF(stype, sname));
		stype += 1;
	}

	if (sname == NULL) {
		TALLOC_FREE(base);
		return NT_STATUS_NO_MEMORY;
	}

	if (sname[0] == '\0') {
		/*
		 * no stream name, so no stream
		 */
		goto done;
	}

	if (pstream != NULL) {
		stream = talloc_asprintf(mem_ctx, "%s:%s", sname, stype);
		if (stream == NULL) {
			TALLOC_FREE(sname);
			TALLOC_FREE(base);
			return NT_STATUS_NO_MEMORY;
		}
		/*
		 * upper-case the type field
		 */
		strupper_m(strchr_m(stream, ':')+1);
	}

 done:
	if (pbase != NULL) {
		*pbase = base;
	}
	if (pstream != NULL) {
		*pstream = stream;
	}
	return NT_STATUS_OK;
}

bool is_valid_policy_hnd(const POLICY_HND *hnd)
{
	POLICY_HND tmp;
	ZERO_STRUCT(tmp);
	return (memcmp(&tmp, hnd, sizeof(tmp)) != 0);
}

/****************************************************************
 strip off leading '\\' from a hostname
****************************************************************/

const char *strip_hostname(const char *s)
{
	if (!s) {
		return NULL;
	}

	if (strlen_m(s) < 3) {
		return s;
	}

	if (s[0] == '\\') s++;
	if (s[0] == '\\') s++;

	return s;
}