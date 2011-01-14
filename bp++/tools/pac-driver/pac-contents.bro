# $Id$

@load weird
@load alarm

@load http-request
@load http-reply
    
redef tcp_content_deliver_all_orig = T;
redef tcp_content_deliver_all_resp = T;
redef udp_content_deliver_all_orig = T;
redef udp_content_deliver_all_resp = T;

global pac_out = open("pac-contents.dat");

# kind:
#
#  D : data chunk (TCP)
#  U : UDP packet payload
#  G : gap
#  T : termination of connection.

function output(c: connection, is_orig: bool, contents: string, len: int, kind: string)
{
    local dir: string;
    local fid = fmt("%s:%s-%s:%s", c$id$orig_h, c$id$orig_p, c$id$resp_h, c$id$resp_p);

    if ( kind != "T" ) {
        dir = is_orig ? ">" : "<";
        print pac_out, fmt("# %s %s %d %s %.6f", kind, dir, len, fid, network_time());
    }

    else {
        # Terminate both directions.
        print pac_out, fmt("# %s < %d %s %.6f", kind, len, fid, network_time());
        print pac_out, fmt("# %s > %d %s %.6f", kind, len, fid, network_time());
    }

    if ( (kind == "D" || kind == "U") && len > 0 )
        write_file(pac_out, contents);
}

global established : set[conn_id];

event connection_established(c: connection)
{
    add established[c$id];
}

event connection_state_remove(c: connection)
{
    if ( c$id in established ) {
        output(c, T, "", 0, "T");
        delete established[c$id];
    }
}

event content_gap(c: connection, is_orig: bool, seq: count, length: count)
{
    if ( c$id in established )
        output(c, is_orig, "", length, "G");
}

event tcp_contents(c: connection, is_orig: bool, seq: count, contents: string)
{
    if ( c$id in established )
        output(c, is_orig, contents, |contents|, "D");
}

event udp_contents(u: connection, is_orig: bool, contents: string)
{
    add established[u$id];
    output(u, is_orig, contents, |contents|, "U");
}
