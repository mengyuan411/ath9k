/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1999, Federal University of Pernambuco - Brazil.
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

struct DSShaper *a_dsshaper;
a_dsshaper->received_packets=0;
a_dsshaper->sent_packets=0;
a_dsshaper->shaped_packets=0;
a_dsshaper->dropped_packets=0;
a_dsshaper->max_queue_length=0;
a_dsshaper->flow_id_=0;
a_dsshaper->last_time=0;
a_dsshaper->burst_size_=1;
struct list_head shape_queue;
INIT_LIST_HEAD(&shape_queue);
struct list_head shape_queue_msg;
INIT_LIST_HEAD(&shape_queue_msg);
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

int timer_module(double time_delay,struct timer_list *my_timer)
{
  int ret;

  //printk("Timer module installing\n");

  // my_timer.function, my_timer.data
  setup_timer( my_timer,resume, 0 );

  //printk( "Starting timer to fire in %ld ms (%ld)\n", time_delay,jiffies );
  ret = mod_timer( &my_timer, jiffies + msecs_to_jiffies(time_delay) );
  if (ret) printk("Error in mod_timer\n");

  return 0;
}


void recv(int len, struct ath_softc *sc, struct ath_txq *txq, struct list_head *p, bool internal)
{

	//struct hdr_cmn *hdr = hdr_cmn::access(p); // unsettled
	//printf("[changhua pei][TC-recv   ][%d->%d][id=%d][type=%d][time=%f][eqts_=%f][holts_=%f][wait_time_=%f][retrycnt=%d]\n",hdr->prev_hop_,hdr->next_hop_,hdr->uid_,hdr->ptype_,Scheduler::instance().clock(),hdr->eqts_,hdr->holts_,hdr->holts_-hdr->eqts_,hdr->retrycnt_);
	
	a_dsshaper->received_packets++;

	if (list_empty(&shape_queue)) {
//          There are no packets being shapped. Tests profile.
	    if (in_profile(len)) {
 	        a_dsshaper->sent_packets++;
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
            shape_packet(p);         
	}
}

bool shape_packet(struct list_head *packet,struct ath_softc *sc, struct ath_txq *txq,bool internal,int len)
{
        if (list_length(shape_queue) >= a_dsshaper->max_queue_length) {
	    drop (p);// unsettled how to drop?
	    a_dsshaper->dropped_packets++;
	    return false;
        } 
        //shape_queue.enque(p);
        list_add_tail(packet,shape_queue);
        

        struct packet_msg *msg;
        INIT_LIST_HEAD(&msg->list);//unsettled 
        msg->sc = sc;
        msg->txq = txq;
        msg->internal = internal;
        msg->len = len;
        list_add_tail(msg,shape_queue_msg);


        a_dsshaper->shaped_packets++;
		
		//struct hdr_cmn *hdr = hdr_cmn::access(p);
		//printf("[changhua pei][TC-enque  ][%d->%d][id=%d][type=%d][time=%f][eqts_=%f][holts_=%f][wait_time_=%f][retrycnt=%d]\n",hdr->prev_hop_,hdr->next_hop_,hdr->uid_,hdr->ptype_,Scheduler::instance().clock(),hdr->eqts_,hdr->holts_,hdr->holts_-hdr->eqts_,hdr->retrycnt_);

		
	return true;
}

void schedule_packet(list_head *p,int len)
{
//      calculate time to wake up	
		struct timer_list my_timer;          
        //int packetsize = hdr_cmn::access(p)->size_ * 8 ;
        //double delay = (packetsize - curr_bucket_contents)/peak_;
        double delay  = 1500.0*8.0*0.000001/11; //unsettled where is length?

		//Scheduler& s = Scheduler::instance();
	    //s.schedule(&sh_, p, delay);
	    timer_module(delay,&my_timer);
}


void resume()
{
	struct list_head *p;
	p = shape_queue->next;
	struct list_head *msg;
	msg = shape_queue_msg->next;
	if (!list_empty(&shape_queue)){
			//p = shape_queue.deque();
		list_del(p);
		list_del(msg);
	}else{
		printf("[changhua pei][Error     ][the queue is empty!]\n");
		return;
	}

	//struct hdr_cmn *hdr = hdr_cmn::access(p);
	//printf("[changhua pei][TC-resume ][%d->%d][id=%d][type=%d][time=%f][eqts_=%f][holts_=%f][wait_time_=%f][retrycnt=%d]\n",hdr->prev_hop_,hdr->next_hop_,hdr->uid_,hdr->ptype_,Scheduler::instance().clock(),hdr->eqts_,hdr->holts_,hdr->holts_-hdr->eqts_,hdr->retrycnt_);
	
	if (in_profile(len)) {
            a_dsshaper->sent_packets++;
            ath_tx_txqaddbuf(msg->sc, msg->txq, p, msg->internal);
            //target_->recv(p,(Handler*) NULL);  //unsettled why recv? 

	} else {
		//printf("[changhua pei][TC-resume0][%d->%d][id=%d][puid_=%d][schedule until the packet is sent out!]\n",hdr->prev_hop_,hdr->next_hop_,hdr->uid_, p->uid_);
        schedule_packet(p,len);
		return;
	} 

	if (!list_empty(&shape_queue)) {  //why don't check the bucket again?
//         There are packets in the queue. Schedule the first one.
           Packet *first_p = shape_queue.lookup(0);
           /*Note that: here the bucket may be filled quickly so that the first packet in the queue can be 
		    * quickly sent out
			*/
		   Scheduler& s = Scheduler::instance();
		   s.schedule(&sh_, first_p, 0);         
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
	
	
	if (packetsize > a_dsshaper->curr_bucket_contents)
		return false;
	else {
		a_dsshaper->curr_bucket_contents -= packetsize ;
		return true ;
	}
}

void update_bucket_contents()
{
//      I'm using the token bucket implemented by Sean Murphy
        
	//double current_time = Scheduler::instance().clock() ;
	void do_gettimeofday(struct timeval *tv);
	double current_time = (double) tv->tv_sec;

	
	double added_bits = (current_time - a_dsshaper->last_time) * peak_ ; //unsettled

	a_dsshaper->curr_bucket_contents += (int) (added_bits + 0.5);
	if (a_dsshaper->curr_bucket_contents > a_dsshaper->burst_size_)
		a_dsshaper->curr_bucket_contents=a_dsshaper->burst_size_ ;
	a_dsshaper->last_time = current_time ;


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