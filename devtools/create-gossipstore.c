#include "config.h"
#include <ccan/crc32c/crc32c.h>
#include <ccan/err/err.h>
#include <ccan/opt/opt.h>
#include <ccan/read_write_all/read_write_all.h>
#include <common/gossip_store.h>
#include <fcntl.h>
#include <gossipd/gossip_store_wiregen.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <wire/peer_wire.h>


struct scidbro {
	struct short_channel_id scid;
        struct amount_bro bro;
};

/* read scid,broneess csv file and create return an array of scidbro pointers */
static struct scidbro *load_csv_file(FILE *scidf)
{
        int n, r;
	char title[15];
	int i = 0;
	struct scidbro *scidbros;
        /* max characters is 8 (0xffffff) + 8 for tx + 5 (0xffffff) for outputs (0xffff) + 2 (x's) */
        char str[23];

        if (fscanf(scidf, "%d\n", &n) != 1)
                err(1, "reading number of entries from csv failed");

	scidbros = tal_arr(NULL, struct scidbro, n);
        r = fscanf(scidf, "%5s ,%8s\n", title, &title[6]);
	if (r != 2 || strcmp(title, "scid") != 0 || strcmp(&title[6], "broneess") != 0)
		err(1, "reading 'scid ,broneess' from csv failed");

        while(fscanf(scidf, "%s ,%"SCNu64"\n", str, &scidbros[i].bro.broneess) == 2 ) { /* Raw: read from file */
		if (!short_channel_id_from_str(str, strlen(str), &scidbros[i].scid))
			err(1, "failed to make scid struct");
		i++;
	}
	return scidbros;
}

static void write_outmsg(int outfd, const u8 *outmsg, u32 timestamp)
{
	struct gossip_hdr hdr;

	hdr.len = cpu_to_be32(tal_count(outmsg));
	hdr.crc = cpu_to_be32(crc32c(timestamp, outmsg, tal_count(outmsg)));
	hdr.timestamp = cpu_to_be32(timestamp);

	if (!write_all(outfd, &hdr, sizeof(hdr))
	    || !write_all(outfd, outmsg, tal_count(outmsg)))
		err(1, "Writing output");
}

static u32 get_update_timestamp(const u8 *msg, struct short_channel_id *scid)
{
	secp256k1_ecdsa_signature sig;
	struct brocoin_blkid chain_hash;
	u32 timestamp;
	u8 u8_ignore;
	u16 u16_ignore;
	u32 u32_ignore;
	struct amount_mbro mbro;

	if (fromwire_channel_update(msg, &sig, &chain_hash, scid,
				    &timestamp, &u8_ignore, &u8_ignore,
				    &u16_ignore, &mbro, &u32_ignore,
				    &u32_ignore))
		return timestamp;
	errx(1, "Invalid channel_update");
}

static u32 get_node_announce_timestamp(const u8 *msg)
{
	secp256k1_ecdsa_signature sig;
	u32 timestamp;
	struct node_id id;
	u8 rgb_color[3], alias[32];
	u8 *features, *addresses;
	struct tlv_node_ann_tlvs *na_tlvs;

	if (fromwire_node_announcement(tmpctx, msg, &sig, &features, &timestamp,
				       &id, rgb_color, alias, &addresses,
				       &na_tlvs))
		return timestamp;

	errx(1, "Invalid node_announcement");
}

int main(int argc, char *argv[])
{
	u8 version;
	beint16_t be_inlen;
	struct amount_bro bro;
	bool verbose = false;
	char *infile = NULL, *outfile = NULL, *csvfile = NULL, *cbro = NULL;
	int infd, outfd, scidi = 0, channels = 0, nodes = 0, updates = 0;
	struct scidbro *scidbros = NULL;
	const u8 *last_announce = NULL;
	unsigned max = -1U;

	setup_locale();

	opt_register_noarg("--verbose|-v", opt_set_bool, &verbose,
			   "Print progress to stderr");
	opt_register_arg("--output|-o", opt_set_charp, NULL, &outfile,
			 "Send output to this file instead of stdout");
	opt_register_arg("--input|-i", opt_set_charp, NULL, &infile,
			 "Read input from this file instead of stdin");
	opt_register_arg("--csv", opt_set_charp, NULL, &csvfile,
			 "Input for 'scid, broshis' csv");
	opt_register_arg("--bro", opt_set_charp, NULL, &cbro,
			 "default bronees value if --csv flag not present");
	opt_register_arg("--max", opt_set_uintval, opt_show_uintval, &max,
			 "maximum number of messages to read");
	opt_register_noarg("--help|-h", opt_usage_and_exit,
			   "Create gossip store, from be16 / input messages",
			   "Print this message.");

	opt_parse(&argc, argv, opt_log_stderr_exit);


        if (csvfile && !cbro) {
       	        FILE *scidf;
		scidf = fopen(csvfile, "r");
		if (!scidf)
			err(1, "opening %s", csvfile);
                scidbros = load_csv_file(scidf);
	        fclose(scidf);
	} else if (cbro && !csvfile) {
		if (!parse_amount_bro(&bro, cbro, strlen(cbro))) {
		        errx(1, "Invalid bronees amount %s", cbro);
		}
	}
	else {
		err(1, "must contain either --bro xor --csv");
	}

	if (infile) {
		infd = open(infile, O_RDONLY);
		if (infd < 0)
			err(1, "opening %s", infile);
	}
	else
		infd = STDIN_FILENO;

	if (outfile) {
		outfd = open(outfile, O_WRONLY|O_TRUNC|O_CREAT, 0666);
		if (outfd < 0)
			err(1, "opening %s", outfile);
	} else
		outfd = STDOUT_FILENO;

	version = GOSSIP_STORE_VERSION;
	if (!write_all(outfd, &version, sizeof(version)))
		err(1, "Writing version");

	while (read_all(infd, &be_inlen, sizeof(be_inlen))) {
		u32 msglen = be16_to_cpu(be_inlen);
		u8 *inmsg = tal_arr(NULL, u8, msglen);
		u32 timestamp;
		struct short_channel_id scid;

		if (!read_all(infd, inmsg, msglen))
			err(1, "Only read partial message");

		if (verbose)
			fprintf(stderr, "%s\n",
				peer_wire_name(fromwire_peektype(inmsg)));

		switch (fromwire_peektype(inmsg)) {
		case WIRE_CHANNEL_ANNOUNCEMENT:
			if (scidbros) {
				/* We ignore these; we just want scid */
				secp256k1_ecdsa_signature sig;
				u8 *features;
				struct brocoin_blkid hash;
				struct node_id id;
				struct pubkey pubkey;

				if (!fromwire_channel_announcement(inmsg,
								   inmsg,
								   &sig,
								   &sig,
								   &sig,
								   &sig,
								   &features,
								   &hash,
								   &scid,
								   &id,
								   &id,
								   &pubkey,
								   &pubkey))
					errx(1, "bad channel_announcement");
			        if (!short_channel_id_eq(&scid, &scidbros[scidi].scid))
					errx(1, "scid of channel_announcement does not match scid in csv");
				if (last_announce)
					errx(1, "Expected update before announce");
				last_announce = inmsg;
				inmsg = NULL;
			}
			break;

		case WIRE_CHANNEL_UPDATE:
			/* We assume update immediately follows announcement */
			timestamp = get_update_timestamp(inmsg, &scid);
			if (last_announce) {
				if (scidbros && !short_channel_id_eq(&scid,
							 &scidbros[scidi].scid))
					errx(1, "scid of channel_update does not match scid in csv");

				/* Now we have timestamp, write out announce */
				/* First write announce */
				write_outmsg(outfd, last_announce, timestamp);
				last_announce = tal_free(last_announce);
				channels += 1;
				/* Now write amount */
				write_outmsg(outfd,
					     towire_gossip_store_channel_amount(inmsg,
										scidbros ? scidbros[scidi].bro: bro),
					     0);
				scidi++;
			}
			write_outmsg(outfd, inmsg, timestamp);
			updates += 1;
			break;

		case WIRE_NODE_ANNOUNCEMENT:
			timestamp = get_node_announce_timestamp(inmsg);
			write_outmsg(outfd, inmsg, timestamp);
			nodes += 1;
			break;

		default:
			warnx("Unknown message %u (%s)", fromwire_peektype(inmsg),
			      peer_wire_name(fromwire_peektype(inmsg)));
			tal_free(inmsg);
			continue;
		}
		tal_free(inmsg);
		if (--max == 0)
			break;
	}
	fprintf(stderr, "channels %d, updates %d, nodes %d\n", channels, updates, nodes);
	tal_free(scidbros);
	return 0;
}
