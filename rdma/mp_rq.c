#include <infiniband/verbs.h>
#include <infiniband/verbs_exp.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define PORT_NUM 1

#define ENTRY_SIZE      9000 /* maximum size of each received packet - set to jumbo frame */
#define RQ_NUM_DESC     512  /* maximum receive ring length without processing */

/* mac we are listening on */
#define DEST_MAC {0x00, 0x01, 0x02, 0x03, 0x04, 0x05}


int main() {
    struct ibv_device **dev_list;
    struct ibv_device *ib_dev;
    struct ibv_context *context;
    struct ibv_pd *pd;
	int ret;

	
    /* get list of offload capable devices */
    dev_list = ibv_get_device_list(NULL);
    if (!dev_list) {
        perror("Failed to get IB devices list");
        exit(1);
    }



    /* always use first device in list */
    ib_dev = dev_list[0];
    if (!ib_dev) {
        fprintf(stderr, "IB device not found\n");
        exit(1);
    }

    /* get conetext to device - needed for resource tracking and further operations */
    context = ibv_open_device(ib_dev);
    if (!context) {
        fprintf(stderr, "Couldn't get context for %s\n",
                ibv_get_device_name(ib_dev));
        exit(1);
    }

    /* allocate a protection domain to group memory regions and rings */
    pd = ibv_alloc_pd(context);
    if (!pd) {
        fprintf(stderr, "Couldn't allocate PD\n");
        exit(1);
    }


    /* create completion queue */
    struct ibv_cq *cq;
	cq = ibv_create_cq(context, RQ_NUM_DESC, NULL, NULL, 0);
	if (!cq) {
		fprintf(stderr, "Couldn't create CQ %d\n", errno);
        exit (1);
	}

    struct ibv_qp *qp;

    struct ibv_qp_init_attr qp_init_attr = {
        .qp_context = NULL,
        /* report receive completion to cq */
        .send_cq = cq,
        .recv_cq = cq,
        
        .cap = { 
            /* no send ring */
            .max_send_wr = 0,

            /* maximum number of packets in ring */
            .max_recv_wr = RQ_NUM_DESC,
            /* only one pointer per descriptor */
            .max_recv_sge = 1,

        },

        .qp_type = IBV_QPT_RAW_PACKET,
    };

    /* create receive ring */
    qp = ibv_create_qp(pd, &qp_init_attr);
    if (!qp)  {
        fprintf(stderr, "Couldn't create RSS QP\n");
        exit(1);
    }

    struct ibv_qp_attr qp_attr;
    int qp_flags;

    memset(&qp_attr, 0, sizeof(qp_attr));

    /* initialize receive ring + assign port */
    qp_flags = IBV_QP_STATE | IBV_QP_PORT;
    qp_attr.qp_state        = IBV_QPS_INIT;
    qp_attr.port_num        = 1;
    ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
    if (ret < 0) {
        fprintf(stderr, "failed modify qp to init\n");
        exit(1);
    }

    memset(&qp_attr, 0, sizeof(qp_attr));

    /* 
     * move ring state to ready to receive
     * needed to be able to receive packets
     * in ring
     */
    qp_flags = IBV_QP_STATE;
    qp_attr.qp_state = IBV_QPS_RTR;
    ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
    if (ret < 0) {
        fprintf(stderr, "failed modify qp to recevie\n");
        exit(1);
    }

    /* maximum size of data to be accessed by hardware */
    int buf_size = ENTRY_SIZE*RQ_NUM_DESC;
    void *buf;
    buf = malloc(buf_size);
    if (!buf) {
        fprintf(stderr, "Coudln't allocate memory\n");
        exit(1);
    }

    struct ibv_mr *mr;

    /* register user memory so it can be accessed by hw directly */
    mr = ibv_reg_mr(pd, buf, buf_size, IBV_ACCESS_LOCAL_WRITE);
    if (!mr) {
        fprintf(stderr, "Couldn't register mr\n");
        exit(1);
    }

    int n;
    struct ibv_sge sg_entry;
    struct ibv_recv_wr wr, *bad_wr;

    
    /* pointer to packet buffer size and memory key of each packet buffer */
    sg_entry.length = ENTRY_SIZE;
    sg_entry.lkey   = mr->lkey;


    /* 
     * descriptor for receive transaction - details:
     *      -how many pointers to receive buffers to use
     *      -if this is a single descriptor or a list (next == NULL single)
     */
    wr.num_sge  = 1;
    wr.sg_list = &sg_entry;
    wr.next     = NULL;

    for (n = 0; n < RQ_NUM_DESC; n++) {
        /* each descriptor points to max mtu size buffer */
        sg_entry.addr   = (uint64_t)buf + ENTRY_SIZE*n;
        
        /* index of descriptor returned when packet arrives */
        wr.wr_id = n;
    
        /* post recieve buffer to ring */
        ibv_post_recv(qp, &wr, &bad_wr);
    }


    /* 
     * register steering rule to intercept packet to DEST_MAC 
     * and place packet in ring pointed by ->qp
     */
    struct raw_eth_flow_attr {
        struct ibv_flow_attr        attr;
        struct ibv_flow_spec_eth    spec_eth;
    } __attribute__((packed)) flow_attr = {
        .attr = {
            .comp_mask  = 0,
            .type       = IBV_FLOW_ATTR_NORMAL,
            .size       = sizeof(flow_attr),
            .priority   = 0,
            .num_of_specs   = 1,
            .port       = PORT_NUM,
            .flags      = 0,
        },
        .spec_eth = {
            .type   = IBV_EXP_FLOW_SPEC_ETH,
            .size   = sizeof(struct ibv_flow_spec_eth),
            .val = {
                .dst_mac = DEST_MAC,
                .src_mac = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
                .ether_type = 0,
                .vlan_tag = 0,
            },
            .mask = {
                .dst_mac = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
                .src_mac = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
                .ether_type = 0,
                .vlan_tag = 0,
            }
        }
    };

    struct ibv_flow *eth_flow;
    
    /* create steering rule */
    eth_flow = ibv_create_flow(qp, &flow_attr.attr);
    if (!eth_flow) {
        fprintf(stderr, "Couldn't attach steering flow\n");
        exit(1);
    }
    
    int msgs_completed;
    struct ibv_wc wc;

    while(1) {
        /* wait for completion */
        msgs_completed = ibv_poll_cq(cq, 1, &wc);
        if (msgs_completed > 0) {
            /* 
             * completion includes:
             *   -status of descriptor 
             *   -index of descriptor completing
             *   -size of the incoming packets
             */
            printf("message %ld received size %d\n", wc.wr_id, wc.byte_len);
            sg_entry.addr = (uint64_t)buf + wc.wr_id*ENTRY_SIZE;
            wr.wr_id = wc.wr_id;
            /* after processed need to post back buffer */
            ibv_post_recv(qp, &wr, &bad_wr);
        } else if (msgs_completed < 0) {
            printf("Polling error\n");
            exit(1);
        }
    }


    printf("We are done\n");

    return 0;
}


