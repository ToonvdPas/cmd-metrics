#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "mempool.h"
#include "inode_stats.h"

void read_proc_file(char *fname, char **buf, long *size) {
	FILE *fd;
	char *newptr;
	long read;

	if (*buf == NULL) {
		if ((*buf = malloc(*size + 1)) == NULL) {
			printf("ERROR - malloc(%ld) failed, %d - %s\n", *size, errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	while (1) {
		if ((fd = fopen(fname, "r")) == NULL) {
			printf("ERROR - fopen(%s, \"r\") failed, %d - %s\n", fname, errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		read = fread(*buf, sizeof(char), *size, fd);
		memset(*buf + read, 0, 1);
		if (feof(fd)) {
			// De data past in de buffer.
			fclose(fd);
			return;
		} else {
			// De data paste niet in de buffer.
			// We willen de complete file in een enkele buffer hebben.
			// Maak de buffer 2x zo groot en probeer opnieuw.
			fclose(fd);
			*size *= 2;
			newptr = realloc(*buf, *size + 1);
			if (newptr) {
				*buf = newptr;
			} else {
				printf("ERROR - realloc(%ld) failed, %d - %s\n", *size + 1, errno, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
	}
}

int ino_hashfn(unsigned int ino) {
	int val = (ino >> 24) ^ (ino >> 16) ^ (ino >> 8) ^ ino;

	return val & (INO_HASH_SIZE - 1);
}

void sock_ino_traverse_hash_table(sock_ino_ent_t *hash_array[INO_HASH_SIZE]) {
	sock_ino_ent_t *p;

	for (int i=0; i<INO_HASH_SIZE; i++) {
		printf("sock_ino_traverse_hash_table() - hash_array[%d] points at %p\n", i, hash_array[i]);
		if (hash_array[i]) {
			p = hash_array[i];
			while (p) {
				printf("sock_ino_traverse_hash_table()       p->ino==%d, p->uid==%d, p->next==%p\n", p->ino, p->uid, p->next);
				p = p->next;
			}
		}
	}
}

void sock_ino_fix_pointers(POOL *pool_ino_old, POOL *pool_ino_new, sock_ino_ent_t *hash_array[INO_HASH_SIZE]) {
	signed long long pool_ino_offset;
	sock_ino_ent_t *p;
	char *pool_ino_old_c, *pool_ino_new_c;
	char *p_c;

	pool_ino_old_c = (char *) pool_ino_old;
	pool_ino_new_c = (char *) pool_ino_new;
	pool_ino_offset = pool_ino_new_c - pool_ino_old_c;
//	printf("sock_ino_fix_pointers(%p, %p, %p) - pool_ino_offset: %lld\n", pool_ino_old, pool_ino_new, hash_array, pool_ino_offset);
	if (pool_ino_offset) {
		// de realloc() call heeft de memory-pool verplaatst naar een nieuwe locatie.
		// alle bestaande pointers naar de memory-pool moeten gefixt worden.
		for (int i=0; i<INO_HASH_SIZE; i++) {
			if (hash_array[i]) {
				p_c = (char *) hash_array[i] + pool_ino_offset;
				p = (sock_ino_ent_t *) p_c;
//				printf("sock_ino_fix_pointers() - entering while-loop for hash_array[%d] old p==%p, new p==%p, p_c==%p, p->next==%p\n", i, hash_array[i], p, p_c, p->next);
				hash_array[i] = p;
				while (p) {
					if (p->next) {
						p_c = (char *) p->next + pool_ino_offset;
						p->next = (sock_ino_ent_t *) p_c;
//						printf("sock_ino_fix_pointers() - for p (%p) fixed p->next to %p\n", p, p->next);
					}
//					printf("sock_ino_fix_pointers() - p->next==%p, p->ino==%d, p->uid==%d\n", p->next, p->ino, p->uid);
					p = p->next;
//					printf("sock_ino_fix_pointers() - at end of while-block, p == %p\n", p);
				}
			}
		}
	}
}

void sock_ino_add(sock_ino_ent_t *hash_array[INO_HASH_SIZE], POOL **pool_ino,
                      unsigned int ino, unsigned int uid,
                      unsigned int addr_loc, unsigned int port_loc,
                      unsigned int addr_rem, unsigned int port_rem,
		      unsigned int state) {
	sock_ino_ent_t *p, *p2;
	POOL *pool_ino_old = *pool_ino;
	POOL *pool_ino_new = *pool_ino;

	// Maak een nieuwe node aan
	p = pool_alloc(*pool_ino, (sizeof(sock_ino_ent_t)));
	if (!p) {
//		sock_ino_traverse_hash_table(hash_array);
		if ((*pool_ino = pool_extend(*pool_ino)) == NULL) {
			printf("ERROR - failed pool_extend() in sock_ino_add()\n");
			exit(EXIT_FAILURE);
		} else {
			pool_ino_new = *pool_ino;
			sock_ino_fix_pointers(pool_ino_old, pool_ino_new, hash_array);
	
			if ((p = pool_alloc(*pool_ino, sizeof(sock_ino_ent_t))) == NULL) {
				printf("ERROR - failed pool_alloc sock_ino_ent_t (%ld bytes)\n", sizeof(sock_ino_ent_t));
				exit(EXIT_FAILURE);
			}
		}
	}

	// Vul de node met gegevens
	p->next     = NULL;
	p->ino      = ino;
	p->uid      = uid;
	p->addr_loc = addr_loc;
	p->port_loc = port_loc;
	p->addr_rem = addr_rem;
	p->port_rem = port_rem;
	p->state    = state;

	// Hang de reeds bestaande linked list (=de hash-bucket) aan p->next
	p->next = hash_array[ino_hashfn(ino)];

	// Wijs het adres van de nieuwe node toe aan de hash-bucket
	hash_array[ino_hashfn(ino)] = p;
}

sock_ino_ent_t * sock_ino_find(sock_ino_ent_t *hash_array[INO_HASH_SIZE], unsigned int ino) {
	sock_ino_ent_t *p;
	int cnt = 0;

	// Wanneer het inode-nummer 0 is, skippen we hem.
	// Alle inodes in de status TIME_WAIT krijgen namelijk inode-nummer 0 toebedeeld,
	// want die zijn niet meer gekoppeld aan een owner (een proces).
	if (!ino)
		return NULL;

	// Selecteer de hash-bucket voor deze inode
	p = hash_array[ino_hashfn(ino)];

	// Scan de hash-bucket
	while (p && cnt==0) {             // inode-nummers zijn uniek, dus we stoppen na een match
		if (p->ino == ino) {
			cnt++;
		} else {
			p = p->next;
		}
	}
	return p;
}

void sock_ino_print(sock_ino_ent_t *p, int pid) {
	char addr_loc_str[INET_ADDRSTRLEN], addr_rem_str[INET_ADDRSTRLEN];

	inet_ntop(AF_INET, &p->addr_loc, addr_loc_str, INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &p->addr_rem, addr_rem_str, INET_ADDRSTRLEN);
	printf("Found pid: %5d ino: %9d uid: %d local addr: %15s:%-5d remote addr: %15s:%-5d state: %2d\n",
		pid, p->ino, p->uid,
		addr_loc_str, p->port_loc,
		addr_rem_str, p->port_rem, p->state);
}

void sock_aggr_print(sock_aggr_t *s) {
	printf("cnt: %5ld established: %5ld close_wait: %5ld listen: %5ld rest: %5ld\n", s->sock_total,
	        s->state.established, s->state.close_wait, s->state.listener, s->state.rest);
}

void sock_ino_build_hash_table(sock_ino_ent_t *hash_array[INO_HASH_SIZE], POOL **pool_ino, char **read_buf, long *buflen) {
	char *net = getenv("PROC_TCP") ? : "/proc/net/tcp";
	unsigned int addr_loc, port_loc, addr_rem, port_rem, state, uid, ino;
	int matchcnt;

	for (int i=0; i<INO_HASH_SIZE; i++) {
		hash_array[i] = NULL;
	}

	// Plaats het volledige bestand /proc/net/tcp in één grote buffer.
	read_proc_file(net, read_buf, buflen);

	// Lees de buffer en bouw daarmee de hash-table op.
	char * line = strtok(*read_buf, "\n");
	line  = strtok(NULL, "\n");       // Skip de eerste regel (=kopregel).
	while(line) {
		matchcnt = sscanf(line, " %*s %8x:%4x %8x:%4x %2x %*8x:%*8x %*2x:%*8x %*8x %u %*d %u ",
			&addr_loc, (unsigned int *) &port_loc,
			&addr_rem, (unsigned int *) &port_rem,
			&state, &uid, &ino);

		// Sockets met inode number 0 zijn niet owned door een proces,
		// dus die hoeven we niet op te nemen in de hash table.
		if (ino) {
			sock_ino_add(hash_array, pool_ino, ino, uid, addr_loc, port_loc, addr_rem, port_rem, state);
		}

		line  = strtok(NULL, "\n");
	}
}

void sock_ino_destroy_hash_table(sock_ino_ent_t *hash_array[INO_HASH_SIZE], POOL *pool_ino) {
	pool_reset(pool_ino);
	for (int i=0; i<INO_HASH_SIZE; i++) {
		hash_array[i] = NULL;
	}
}

sock_aggr_t * sock_ino_gather_cmd_stats(sock_ino_ent_t *hash_array[INO_HASH_SIZE], POOL **pool_ino, char *cmd) {
	const char *root = getenv("PROC_ROOT") ? : "/proc/";
	struct dirent *d;
	char name[1024];
	int nameoff;
	DIR *dir;
	sock_ino_ent_t *p;
	static sock_aggr_t *s;

	s = pool_alloc(*pool_ino, (sizeof(sock_aggr_t)));
	if (!s) {
		if ((*pool_ino = pool_extend(*pool_ino)) == NULL) {
			printf("ERROR - failed pool_extend() in sock_ino_gather_cmd_stats()\n");
			exit(EXIT_FAILURE);
		} else {
			if ((s = pool_alloc(*pool_ino, sizeof(sock_aggr_t))) == NULL) {
				printf("ERROR - failed pool_alloc sock_aggr_t (%ld bytes)\n", sizeof(sock_aggr_t));
				exit(EXIT_FAILURE);
			}
		}
	}
	memset(s, 0, sizeof(sock_aggr_t));

	strcpy(name, root);
	if (strlen(name) == 0 || name[strlen(name)-1] != '/')
		strcat(name, "/");

	nameoff = strlen(name);

	// Open de directory /proc/
//	printf("INFO - Opening directory %s\n", name);                                        // DEBUG
	dir = opendir(name);
	if (!dir) {
		printf("ERROR - failed to open %s, %d - %s\n", name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	while ((d = readdir(dir)) != NULL) {
		struct dirent *d1;
		char process[16];
		int pid, pos;
		DIR *dir1;
		char crap;
		
		// Vind alle directories onder /proc/ waarvan de naam een getal is (=PID)
//		printf("INFO - Scanning d_name %s\n", d->d_name);     // DEBUG
		if (sscanf(d->d_name, "%d%c", &pid, &crap) != 1)
			continue;
//		printf("INFO - Found PID %d\n", pid);                 // DEBUG

		// Vraag de cmd-naam op.
		// In geval van een error gaan we gewoon door, want het is altijd mogelijk
		// dat een PID nèt verdwijnt tussen het moment dat we de directory lezen
		// en het moment dat we de file openen om de cmd-naam uit te lezen.
		FILE *fp;
		char tmp[1024];
		snprintf(tmp, sizeof(tmp), "%s/%d/stat", root, pid);
		if ((fp = fopen(tmp, "r")) != NULL) {
			if (fscanf(fp, "%*d (%[^)])", process) == EOF) {
//				fprintf(stderr, "DEBUG: getting the cmd-name of PID %d failed: %d (%s)\n", pid, errno, strerror(errno));
			}
			fclose(fp);
		}

		// Check of dit de gevraagde cmd-naam is, zoek anders verder
		if (strncmp(cmd, process, strlen(process))) {
//			printf("Compare: %s != %s\n", cmd, process);  // DEBUG
			continue;
//		} else {                                              // DEBUG
//			printf("Scanning %s, PID %d\n", cmd, pid);    // DEBUG
		}

		// Open de  de filedescriptor-directory onder de gevonden PID-directory.
		// In geval van een error gaan we gewoon door, want wanneer wanneer je niet
		// als root draait heb je geen toegang tot de FD-directories van alle PID's.
		sprintf(name + nameoff, "%d/fd/", pid);
		pos = strlen(name);
		if ((dir1 = opendir(name)) == NULL)
			continue;

		process[0] = '\0';

		while ((d1 = readdir(dir1)) != NULL) {
			const char *pattern = "socket:[";
			unsigned int ino;
			char lnk[64];
			int fd;
			ssize_t link_len;

			// Vind alle directories in /proc/PID/fd/ waarvan de naam een getal is (=FD)
			if (sscanf(d1->d_name, "%d%c", &fd, &crap) != 1)
				continue;

			sprintf(name+pos, "%d", fd);

			// De FD is een link.  Vraag de naam van de bestemming van de link op.
			// (aan die naam kun je zien of het een socket is, en hij bevat tevens het inode-nummer)
			link_len = readlink(name, lnk, sizeof(lnk)-1);
			if (link_len == -1)
				continue;
			lnk[link_len] = '\0';

			// Ga na of dit een link naar een socket is.
			if (strncmp(lnk, pattern, strlen(pattern)))
				continue;

			// Het is een socket.  Vraag het inode-nummer op.
			sscanf(lnk, "socket:[%u]", &ino);

			// We gaan nu de status van de socket bijzoeken in de hashtabel.
			// Om diverse redenen zal niet elk socket ook voorkomen in de hashtabel.
			// Daarom tellen we hier éérst alvast de socket, zodat er geen ontbreken in de totaaltelling.
			s->sock_total += 1;
			p = sock_ino_find(hash_array, ino);
			if (p) {
				switch (p->state) {
				case TCP_ESTABLISHED: s->state.established += 1;
				                      break;
				case TCP_CLOSE_WAIT:  s->state.close_wait += 1;
				                      break;
				case TCP_LISTEN:      s->state.listener += 1;
				                      break;
				default:              ;  // no-op
				}
//				sock_ino_print(p, pid);                     // DEBUG
//				sock_aggr_print(s);                         // DEBUG
			}
		}
		closedir(dir1);
		s->state.rest = s->sock_total - (s->state.established + s->state.close_wait + s->state.listener);
	}
	closedir(dir);
	return s;
}

#ifdef MODULE_TEST
int main(int argc, char **argv) {
	sock_ino_ent_t *sock_ino_ent_hash[INO_HASH_SIZE];
	sock_aggr_t *s = NULL;

	sock_ino_build_hash_table(sock_ino_ent_hash);
	s = sock_ino_gather_cmd_stats(sock_ino_ent_hash, argv[1]);
	sock_aggr_print(s);
	sock_ino_destroy_hash_table(sock_ino_ent_hash);

	sock_ino_build_hash_table(sock_ino_ent_hash);
	s = sock_ino_gather_cmd_stats(sock_ino_ent_hash, argv[2]);
	sock_aggr_print(s);
	sock_ino_destroy_hash_table(sock_ino_ent_hash);
}
#endif  // MODULE_TEST
