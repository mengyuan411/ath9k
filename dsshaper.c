/*opyright (c) 1999, Federal University of Pernambuco - Brazil.
 * All rights reserved.
 *
 * License is granted to copy, to use, and to make and to use derivative
 * works for research and evaluation purposes.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Carlos Alberto Kamienski <cak@di.ufpe.br>
 *
 */

#include "dsshaper.h"
#include <linux/dma-mapping.h>
#include "ath9k.h"
#include "ar9003_mac.h"
struct timespec last_ack={0}; // for the last packet ack by mengy
int update_te_flag = 0;
int update_tw_flag = 0;
int has_beacon_flag = 0;
int packet_number = 0;
int packet_size_all = 0;
int last_ack_update_flag = 0;
struct timespec this_ack = {0};
struct timespec this_tw = {0};

/*
void DSShaperHandler::handle(Event *e)
{
	shaper_->resume();
}*/


/*
static class DSShaperTclClass : public TclClass {
public:
	DSShaperTclClass() : TclClass("DSShaper") {}
	TclObject* create(int, const char*const*) {
                return (new DSShaper);
        }
} class_dsshaper ;
*/

/*
DSShaper::DSShaper() : Connector(), 
			   received_packets(0),
			   sent_packets(0),
			   shaped_packets(0),
			   dropped_packets(0),
			   max_queue_length(0),
			   flow_id_(0),
			   last_time(0),
                       sh_(this)
{

}
*/
//extern void recv(int len, struct ath_softc *sc, struct ath_txq *txq, struct list_head *p, bool internal);
int list_length(struct list_head *head);
int timer_module(int time_delay,struct timer_list *my_timer);
//extern void recv(int len, struct ath_softc *sc, struct ath_txq *txq, struct list_head *p, bool internal);
bool shape_packet(struct list_head *packet,struct ath_softc *sc, struct ath_txq *txq,bool internal,int len);
void schedule_packet(struct list_head *p,int len);
void resume(void);
bool in_profile(int size);
extern void update_bucket_contents(void);
extern void ath_tx_txqaddbuf(struct ath_softc *sc, struct ath_txq *txq,
                           struct list_head *head, bool internal);
struct DSShaper dsshaper_my = { 0,0,0,0,0,0,{0,0},80000,30};
//extern int flow_peak=11;
//dsshaper_my=(struct DSShaper*)malloc(sizeof(struct DSShaper));
//dsshaper_my->received_packets=0;
//dsshaper_my->sent_packets=0;
//dsshaper_my->shaped_packets=0;
//dsshaper_my->dropped_packets=0;
//dsshaper_my->max_queue_length=0;
//dsshaper_my->flow_id_=0;
//dsshaper_my->last_time=0;
//dsshaper_my->burst_size_=1;
struct list_head shape_queue;
int init_flag = 0;
//INIT_LIST_HEAD(shape_queue);
struct list_head shape_queue_msg;
//INIT_LIST_HEAD(shape_queue_msg);
//&a_dsshaper->shape_queue=NULL;

int list_length(struct list_head *head)
{
	if(list_empty(head)){
		return 0;
	}
	int count = 0;
	struct list_head *p;
	p = head->next;
	while(p){
		count++;
		if(list_is_last(p,head)){
			return count;
		}
		p = p->next;

	}

}

int timer_module(int time_delay,struct timer_list *my_timer)
{
  int ret;

  //printk("Timer module installing\n");

  // my_timer.function, my_timer.data
  setup_timer( my_timer,resume, 0 );

  //printk( "Starting timer to fire in %ld ms (%ld)\n", time_delay,jiffies );
  ret = mod_timer(my_timer, jiffies + msecs_to_jiffies(time_delay) );
  if (ret) printk("Error in mod_timer\n");

  return 0;
}


extern void recv(int len, struct ath_softc *sc, struct ath_txq *txq, struct list_head *p, bool internal)
{

	//struct hdr_cmn *hdr = hdr_cmn::access(p); // unsettled
	//printf("[changhua pei][TC-recv   ][%d->%d][id=%d][type=%d][time=%f][eqts_=%f][holts_=%f][wait_time_=%f][retrycnt=%d]\n",hdr->prev_hop_,hdr->next_hop_,hdr->uid_,hdr->ptype_,Scheduler::instance().clock(),hdr->eqts_,hdr->holts_,hdr->holts_-hdr->eqts_,hdr->retrycnt_);
	if(init_flag == 0){
		INIT_LIST_HEAD(&shape_queue);
		INIT_LIST_HEAD(&shape_queue_msg);
		init_flag=1;
	}				
	dsshaper_my.received_packets++;

	if (list_empty(&shape_queue)) {
//          There are no packets being shapped. Tests profile.
	    if (in_profile(len)) {
 	        dsshaper_my.sent_packets++;
 	        ath_tx_txqaddbuf(sc, txq, p, internal);
	        //target_->recv(p,h);   //unsettled why call this here? 
	    } else {
	        if (shape_packet(p,sc,txq,internal,len))   
				//printf("[changhua pei][TC-recv0  ][%d->%d][id=%d][puid_=%d][schedule until the packet is sent out!]\n",hdr->prev_hop_,hdr->next_hop_,hdr->uid_, p->uid_);			
           	    schedule_packet(p,len);
//           	else
//                  Shaper is being used as a dropper
            } 
  	} else {          	    
//          There are packets being shapped. Shape this packet too.
            shape_packet(p,sc,txq,internal,len);         
	}
}

bool shape_packet(struct list_head *packet,struct ath_softc *sc, struct ath_txq *txq,bool internal,int len)
{
        if (list_length(&shape_queue) >= dsshaper_my.max_queue_length) {
	    //drop (p);// unsettled how to drop?
	    dsshaper_my.dropped_packets++;
	    return false;
        } 
        //shape_queue.enque(p);
        list_add_tail(packet,&shape_queue);
        

        struct packet_msg *msg;
        INIT_LIST_HEAD(&msg->list);//unsettled 
        msg->sc = sc;
        msg->txq = txq;
        msg->internal = internal;
        msg->len = len;
        list_add_tail(&msg->list,&shape_queue_msg);


        dsshaper_my.shaped_packets++;
		
		//struct hdr_cmn *hdr = hdr_cmn::access(p);
		//printf("[changhua pei][TC-enque  ][%d->%d][id=%d][type=%d][time=%f][eqts_=%f][holts_=%f][wait_time_=%f][retrycnt=%d]\n",hdr->prev_hop_,hdr->next_hop_,hdr->uid_,hdr->ptype_,Scheduler::instance().clock(),hdr->eqts_,hdr->holts_,hdr->holts_-hdr->eqts_,hdr->retrycnt_);

		
	return true;
}

void schedule_packet(struct list_head *p,int len)
{
//      calculate time to wake up	
		struct timer_list my_timer;          
        //int packetsize = hdr_cmn::access(p)->size_ * 8 ;
        int delay = (int) (len - dsshaper_my.curr_bucket_contents) * 1000 /flow_peak; //ms
        //double delay  = 1500.0*8.0*0.000001/11; //use msec unsettled where is length?

		//Scheduler& s = Scheduler::instance();
	    //s.schedule(&sh_, p, delay);
	    timer_module(delay,&my_timer);
}


void resume()
{
	struct list_head *p;
	//p = (&shape_queue)->next;
	struct list_head *lh;
	//lh = (&shape_queue_msg)->next;
	struct packet_msg *msg;
	//msg = list_entry(lh,struct packet_msg,list);
	
	if (!list_empty(&shape_queue)){
			//p = shape_queue.deque();
	//	struct list_head *p;
        p = (&shape_queue)->next;
        //struct list_head *lh;
        lh = (&shape_queue_msg)->next;
       // struct packet_msg *msg;
        msg = list_entry(lh,struct packet_msg,list);
		list_del(p);
		list_del(lh);
	}else{
		printk(KERN_DEBUG "[changhua pei][Error     ][the queue is empty!]\n");
		return;
	}

	//struct hdr_cmn *hdr = hdr_cmn::access(p);
	//printf("[changhua pei][TC-resume ][%d->%d][id=%d][type=%d][time=%f][eqts_=%f][holts_=%f][wait_time_=%f][retrycnt=%d]\n",hdr->prev_hop_,hdr->next_hop_,hdr->uid_,hdr->ptype_,Scheduler::instance().clock(),hdr->eqts_,hdr->holts_,hdr->holts_-hdr->eqts_,hdr->retrycnt_);
	
	if (in_profile(msg->len)) {
            dsshaper_my.sent_packets++;
            ath_tx_txqaddbuf(msg->sc, msg->txq, p, msg->internal);
            //target_->recv(p,(Handler*) NULL);  //unsettled why recv? 

	} else {
		//printf("[changhua pei][TC-resume0][%d->%d][id=%d][puid_=%d][schedule until the packet is sent out!]\n",hdr->prev_hop_,hdr->next_hop_,hdr->uid_, p->uid_);
        schedule_packet(p,msg->len);
		return;
	} 

	if (!list_empty(&shape_queue)) {  //why don't check the bucket again?
//         There are packets in the queue. Schedule the first one.
           //Packet *first_p = shape_queue.lookup(0);
           /*Note that: here the bucket may be filled quickly so that the first packet in the queue can be 
		    * quickly sent out
			*/
		   //Scheduler& s = Scheduler::instance();
		   //s.schedule(&sh_, first_p, 0);   
	struct timer_list my_timer;  
	timer_module(1,&my_timer);      
    }   
} 



bool in_profile(int size)
{

	update_bucket_contents() ;

	//struct hdr_cmn *hdr = hdr_cmn::access(p);
	
	//int packetsize = hdr_cmn::access(p)->size_ * 8 ; // in bits
	int packetsize = size * 8;

	
	//printf("[changhua pei][TC-cansend][%d->%d][id=%d][type=%d][time=%f][token_=%d][packetsize_=%d][peak_=%f][burst_size=%d]\n", \
		   //hdr->prev_hop_,hdr->next_hop_,hdr->uid_,hdr->ptype_,Scheduler::instance().clock(), \
		   //curr_bucket_contents,packetsize,peak_,burst_size_);
	
	
	if (packetsize > dsshaper_my.curr_bucket_contents)
		return false;
	else {
		dsshaper_my.curr_bucket_contents -= packetsize ;
		return true ;
	}
}

extern void update_bucket_contents()
{
//      I'm using the token bucket implemented by Sean Murphy
        
	//double current_time = Scheduler::instance().clock() ;
	struct timespec current_time;
	getnstimeofday(&current_time);
	//struct timeval *tv;
	//void do_gettimeofday(tv);
	//u32 current_time = tv.tv_sec * 1000000 + tv.tv_nsec / 1000;
	struct timespec tmp_sub = timespec_sub(current_time,dsshaper_my.last_time);

	
	int added_bits = (int) (tmp_sub.tv_sec * 1000000 + tmp_sub.tv_nsec / 1000) * flow_peak / 1000 ; //unsettled

	dsshaper_my.curr_bucket_contents += (int) (added_bits * 10  + 5) /10 ;
	if (dsshaper_my.curr_bucket_contents > dsshaper_my.burst_size_)
		dsshaper_my.curr_bucket_contents=dsshaper_my.burst_size_ ; //unsettled how to update burst_size
	dsshaper_my.last_time = current_time ;


}

void update_deqrate(int pdelay_sec,int pdelay_nsec, int alldelay_sec,int alldelay_nsec, int pktsize_, int pnumber_)
{
	printk(KERN_DEBUG "pdelay:%ld.%ld,alldelay_:%ld.%ld,pktsize_:%ld,pnumber_:%ld\n",pdelay_sec,pdelay_nsec,alldelay_sec,alldelay_nsec,pktsize_,pnumber_);
}


/*
int DSShaper::command(int argc, const char* const*argv)
{
	if (argc==2) {
		if (strcmp(argv[1],"reset-counters")==0) {
			reset_counters() ;
			return TCL_OK ;
		}
		if (strcmp(argv[1],"get-received-packets")==0) {
			Tcl& tcl = Tcl::instance();
			tcl.resultf("%d",received_packets);
			return TCL_OK ;
		}
		if (strcmp(argv[1],"get-sent-packets")==0) {
			Tcl& tcl = Tcl::instance();
			tcl.resultf("%d",sent_packets);
			return TCL_OK ;
 		}
		if (strcmp(argv[1],"get-shaped-packets")==0) {
			Tcl& tcl = Tcl::instance();
			tcl.resultf("%d",shaped_packets);
			return TCL_OK ;
		}
		if (strcmp(argv[1],"get-dropped-packets")==0) {
			Tcl& tcl = Tcl::instance();
			tcl.resultf("%d",dropped_packets);
			return TCL_OK ;
		}
	}
	if (argc==3) {
		if (strcmp("set-peak-rate", argv[1])==0) {
		    peak_ = atof(argv[2]);
			return TCL_OK ;
		}
		if (strcmp("set-burst-size", argv[1])==0) {
		    burst_size_ = curr_bucket_contents = atoi(argv[2]);
			return TCL_OK ;
		}
		if (strcmp("set-fid", argv[1])==0) {
		    flow_id_ = atoi(argv[2]);
			return TCL_OK ;
		}
		if (strcmp("set-queue-length", argv[1])==0) {
		    max_queue_length = atoi(argv[2]);
			return TCL_OK ;
		}
		return Connector::command(argc,argv) ;
	}
	
	return Connector::command(argc, argv) ;
}
*/

/*void DSShaper::reset_counters()
{
	received_packets = sent_packets = shaped_packets = dropped_packets = 0 ;
}*/


