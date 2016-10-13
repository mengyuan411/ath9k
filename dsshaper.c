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
#include <linux/math64.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/module.h>
/*for timestamp te th tw by mengy*/
#define MS_TO_NS(x)	(x * 1E6L)
struct timespec last_ack={0}; // for the last packet ack by mengy
int update_te_flag = 0;
int update_tw_flag = 0;
int has_beacon_flag = 0;
int packet_number = 0;
int packet_size_all = 0;
int last_ack_update_flag = 0;
struct timespec this_ack = {0};
struct timespec this_tw = {0};
struct hrtimer hr_timer;

/*for update_deqrate*/
//int flow_peak = 80000000;
int ntrans_ = 0;
struct timespec delay_sum_ = {0};
int pktsize_sum_ = 0;
struct timespec checkInterval_ = {0,5000000};
struct timespec checktime_;
int throughput_sum_ = 0;
int alpha_ = 0; //%
int rate_avg_ = 0; //bits/us
int delay_avg_ = 0;
int switchOn_ = 0;
int delay_optimal_ = 2000;//us
int fix_peak = 800000; //bits/s
int flow_peak = 80000000; // bits/s
int beta_ = 100000; //bits/s
int burst_size_ = 80000;// bits
int deltaIncrease_ = 1000000; //bits/s
struct timespec checkThInterval_ = {1,0};
struct timespec checkThtime_ = {0};
int shape_flag = 0;

//extern void recv(int len, struct ath_softc *sc, struct ath_txq *txq, struct list_head *p, bool internal);
int list_length(struct list_head *head);
int timer_module(int time_delay,struct timer_list *my_timer); // time_delay(ms)
void recv(int len, struct ath_softc *sc, struct ath_txq *txq, struct list_head *p, bool internal);
bool shape_packet(struct list_head *packet,struct ath_softc *sc, struct ath_txq *txq,bool internal,int len);
void schedule_packet(struct list_head *p,int len);
void resume(void);
bool in_profile(int size);
void update_bucket_contents(void);
//extern void ath_tx_txqaddbuf(struct ath_softc *sc, struct ath_txq *txq,
  //                         struct list_head *head, bool internal);
struct DSShaper dsshaper_my = { 0,0,0,0,0,0,{0,0},80000,30};
int init_flag = 0; //for the initialize
struct list_head shape_queue; //for the packet queue
struct list_head shape_queue_msg; // for the packet queue msg
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


//INIT_LIST_HEAD(shape_queue);

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

void recv(int len, struct ath_softc* sc, struct ath_txq* txq, struct list_head* p, bool internal)
{

	//unsettled the agg length

	//struct hdr_cmn *hdr = hdr_cmn::access(p); // unsettled
	printk(KERN_DEBUG "[mengy][recv]recv the packet length:%ld\n",len);
	if(init_flag == 0){
		INIT_LIST_HEAD(&shape_queue);
		INIT_LIST_HEAD(&shape_queue_msg);
		//struct timespec now;
                //getnstimeofday(&now);
		//dsshaper_my.last_time = now;		
		init_flag=1;
	}				
	dsshaper_my.received_packets++;
	printk(KERN_DEBUG "[mengy][recv]sent the packet length:%ld\n",len);
	/*just for debug*/
	/*
	int profile_result = in_profile(len);
	printk(KERN_DEBUG "[mengy][recv]profile_result:%ld\n",profile_result);
     	//if (list_empty(&shape_queue))
                //printk(KERN_DEBUG "[mengy][recv]error shape_queue is empty");
    	struct packet_dsshaper* my_packet;
    	my_packet = kzalloc(sizeof(struct packet_dsshaper),GFP_KERNEL);
    	INIT_LIST_HEAD(&my_packet->list);
    	my_packet->packet = p;   
	list_add_tail(&my_packet->list,&shape_queue);
//if (list_empty(&shape_queue))
                //printk(KERN_DEBUG "[mengy][recv]error shape_queue is empty");
	//ath_tx_txqaddbuf(sc, txq, p, internal);
	//list_del(my_packet);
	struct packet_msg *msg;
	msg = kzalloc(sizeof(struct packet_msg),GFP_KERNEL);
        INIT_LIST_HEAD(&msg->list);//unsettled 
        msg->sc = sc;
        msg->txq = txq;
        msg->internal = internal;
        msg->len = len;
        list_add_tail(&msg->list,&shape_queue_msg);
	
	//ath_tx_txqaddbuf(sc, txq, p, internal);
	//list_del(&my_packet->list);
	//list_del(&msg->list);

	struct list_head *p_test;
	struct list_head *lh_msg_test;
	struct list_head *lh_p_test;
	struct packet_msg *msg_test;
	struct packet_dsshaper *packet_dsshaper_test;
	lh_p_test = shape_queue.next;
	//if (list_empty(&shape_queue))
	//	printk(KERN_DEBUG "[mengy][recv]error shape_queue is empty");
        lh_msg_test = shape_queue_msg.next;
	msg_test = list_entry(lh_msg_test,struct packet_msg,list);
	//lh_p_test = 
	packet_dsshaper_test = list_entry(lh_p_test,struct packet_dsshaper,list);
	p_test = packet_dsshaper_test->packet;
	
	//if(p_test == p)
	//printk(KERN_DEBUG "[mengy][recv]get the packet %p , origin packet %p ",p_test,p);
	//else
		//printk(KERN_DEBUG "[mengy][recv]get the packet fail ");
	
	ath_tx_txqaddbuf(msg_test->sc, msg_test->txq,p_test, msg_test->internal);
	//ath_tx_txqaddbuf(msg_test->sc, msg_test->txq,packet_dsshaper_test->packet, msg_test->internal);
	list_del(&packet_dsshaper_test->list);
	kfree(packet_dsshaper_test);
	list_del(&msg_test->list);
	kfree(msg_test);
	return;*/
	/*debug end*/
	//ath_tx_txqaddbuf(sc, txq, p, internal);
	/*if(shape_flag == 0)
	{
	schedule_packet(p,len);
	shape_flag = 1;
	}
	return;*/
	if (list_empty(&shape_queue)) {
//          There are no packets being shapped. Tests profile.
	    if (in_profile(len)) {
 	    	dsshaper_my.sent_packets++;
 	        printk(KERN_DEBUG "[mengy][recv]sent the packet length:%ld\n",len);
 	        ath_tx_txqaddbuf(sc, txq, p, internal);
	    } else if(shape_flag == 0)
	    {			
		printk(KERN_DEBUG "[mengy][recv]shape_flag: %ld\n",shape_flag);
	//	if(shape_packet(p,sc,txq,internal,len)) { 
				//printf("[changhua pei][TC-recv0  ][%d->%d][id=%d][puid_=%d][schedule until the packet is sent out!]\n",hdr->prev_hop_,hdr->next_hop_,hdr->uid_, p->uid_);			
			  schedule_packet(p,len);
			  ath_tx_txqaddbuf(sc, txq, p, internal);
			  shape_flag = 1;
        //   		}
	    } else
	    {
		ath_tx_txqaddbuf(sc, txq, p, internal);	
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
			printk(KERN_DEBUG "[mengy][shape_packet]shape the packet fails length:%ld\n",len);
			dsshaper_my.dropped_packets++;
			return false;
        } 
        //shape_queue.enque(p);
        struct packet_dsshaper* my_packet;
    	my_packet = kzalloc(sizeof(struct packet_dsshaper),GFP_KERNEL);
    	INIT_LIST_HEAD(&my_packet->list);
    	my_packet->packet = packet;   
		list_add_tail(&my_packet->list,&shape_queue);
        //list_add_tail(packet,&shape_queue);
        

        struct packet_msg *msg;
		msg = kzalloc(sizeof(struct packet_msg),GFP_KERNEL);
        INIT_LIST_HEAD(&msg->list);//unsettled 
        msg->sc = sc;
        msg->txq = txq;
        msg->internal = internal;
        msg->len = len;
        list_add_tail(&msg->list,&shape_queue_msg);


        dsshaper_my.shaped_packets++;
		printk(KERN_DEBUG "[mengy][shape_packet]shape the packet success length:%ld\n",len);
		//struct hdr_cmn *hdr = hdr_cmn::access(p);
		//printf("[changhua pei][TC-enque  ][%d->%d][id=%d][type=%d][time=%f][eqts_=%f][holts_=%f][wait_time_=%f][retrycnt=%d]\n",hdr->prev_hop_,hdr->next_hop_,hdr->uid_,hdr->ptype_,Scheduler::instance().clock(),hdr->eqts_,hdr->holts_,hdr->holts_-hdr->eqts_,hdr->retrycnt_);

		
	return true;
}
enum hrtimer_restart my_hrtimer_callback( struct hrtimer *timer )
{
  struct timespec now;
  getnstimeofday(&now);
  printk(KERN_DEBUG "[mengy][schedule_packet]schedule the packet resume for 1ms time:%ld.%ld\n",now.tv_sec,now.tv_nsec);
  printk(KERN_DEBUG "my_hrtimer_callback called (%ld).\n", jiffies );
  int ret;
  shape_flag=0;
  //ret = hrtimer_cancel(&hr_timer );
  //if (ret) printk(KERN_DEBUG "The timer was still in use...\n");

  //printk(KERN_DEBUG "HR Timer module uninstalling\n");
  //shape_flag=0;
  return HRTIMER_NORESTART;
}
//struct hrtimer hr_timer;
void schedule_packet(struct list_head *p,int len)
{
//      calculate time to wake up	
	//struct hrtimer hr_timer;
	ktime_t ktime;
	unsigned long delay_in_ms = 1L;
	printk(KERN_DEBUG "HR Timer module installing\n");
	ktime = ktime_set( 0, 100000);
	hrtimer_init( &hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	hr_timer.function = &my_hrtimer_callback;
	printk( KERN_DEBUG "Starting timer to fire in %ldms (%ld)\n", delay_in_ms, jiffies );
	hrtimer_start( &hr_timer, ktime, HRTIMER_MODE_REL );
  	struct timespec now;
        getnstimeofday(&now);
        printk(KERN_DEBUG "[mengy][schedule_packet]schedule the packet begin for 1ms time:%ld.%ld\n",now.tv_sec,now.tv_nsec);
	
	return;
	struct timer_list my_timer;          
        //int packetsize = hdr_cmn::access(p)->size_ * 8 ;
        //int delay = (int) (len * 8 - dsshaper_my.curr_bucket_contents) * 1000 /flow_peak; //ms
         //use msec unsettled where is length?
	int delay = 1;
		//Scheduler& s = Scheduler::instance();
	    //s.schedule(&sh_, p, delay);
	// struct timespec now;
         //getnstimeofday(&now);
	
	    //printk(KERN_DEBUG "[mengy][schedule_packet]schedule the packet begin for 1ms time:%ld.%ld\n",now.tv_sec,now.tv_nsec);
	timer_module(delay,&my_timer);
	//struct timespec now;
        //getnstimeofday(&now);
	//printk(KERN_DEBUG "[mengy][schedule_packet]schedule the packet begin for 1ms time:%ld.%ld\n",now.tv_sec,now.tv_nsec);
}


void resume()
{
	struct timespec now;
         getnstimeofday(&now);
	printk(KERN_DEBUG "[mengy][schedule_packet]schedule the packet resume for 1ms time:%ld.%ld\n",now.tv_sec,now.tv_nsec);
	spinlock_t lock;
	spin_lock_init(&lock); 
	
	spin_lock_bh(&lock);
	struct packet_msg *msg_resume;
	struct packet_dsshaper *packet_dsshaper_resume;
	struct list_head *lh_msg_resume;
	struct list_head *lh_p_resume;
	if (!list_empty(&shape_queue)){

        	lh_p_resume = shape_queue.next;
        	lh_msg_resume = shape_queue_msg.next;
        	msg_resume = list_entry(lh_msg_resume,struct packet_msg,list);
		packet_dsshaper_resume = list_entry(lh_p_resume,struct packet_dsshaper,list);
		printk(KERN_DEBUG "[mengy][resume]resume the packet length:%ld\n",msg_resume->len);

	}else{
		printk(KERN_DEBUG "[mengy][Error     ][the queue is empty!]\n");
		spin_unlock_bh(&lock);
		return;
	}

	//struct hdr_cmn *hdr = hdr_cmn::access(p);
	//printf("[changhua pei][TC-resume ][%d->%d][id=%d][type=%d][time=%f][eqts_=%f][holts_=%f][wait_time_=%f][retrycnt=%d]\n",hdr->prev_hop_,hdr->next_hop_,hdr->uid_,hdr->ptype_,Scheduler::instance().clock(),hdr->eqts_,hdr->holts_,hdr->holts_-hdr->eqts_,hdr->retrycnt_);
	
//	if (in_profile(msg_resume->len)) {
	if(true){
            	dsshaper_my.sent_packets++;
            	printk(KERN_DEBUG "[mengy][resume]resume and sent the packet length:%ld\n",msg_resume->len);
            	//printk(KERN_DEBUG "[mengy][schedule_packet]schedule the packet resume for 1ms time:%ld.%ld\n",now.tv_sec,now.tv_nsec);
		ath_tx_txqaddbuf(msg_resume->sc, msg_resume->txq, packet_dsshaper_resume->packet, msg_resume->internal);
            	list_del(lh_p_resume);
		list_del(lh_msg_resume);
		kfree(msg_resume);
		kfree(packet_dsshaper_resume);
		spin_unlock_bh(&lock);
	//	shape_flag =0;
		return;
            //target_->recv(p,(Handler*) NULL);  //unsettled why recv? 

	} else {
		//printf("[changhua pei][TC-resume0][%d->%d][id=%d][puid_=%d][schedule until the packet is sent out!]\n",hdr->prev_hop_,hdr->next_hop_,hdr->uid_, p->uid_);
        	printk(KERN_DEBUG "[mengy][resume]resume and schedule the packet length:%ld\n",msg_resume->len);
        	spin_unlock_bh(&lock);
		schedule_packet(packet_dsshaper_resume->packet,msg_resume->len);
		//spin_unlock_bh(&lock);
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
		printk(KERN_DEBUG "[mengy][resume]sent success and schedule again\n");   
		struct timer_list my_timer;  
		timer_module(1,&my_timer);      
    }   
} 



bool in_profile(int size)
{
	printk(KERN_DEBUG "[mengy][in_profile] call in_profile");	

	update_bucket_contents() ;

	//struct hdr_cmn *hdr = hdr_cmn::access(p);
	
	//int packetsize = hdr_cmn::access(p)->size_ * 8 ; // in bits
	int packetsize = size * 8;

	
	//printf("[changhua pei][TC-cansend][%d->%d][id=%d][type=%d][time=%f][token_=%d][packetsize_=%d][peak_=%f][burst_size=%d]\n", \
		   //hdr->prev_hop_,hdr->next_hop_,hdr->uid_,hdr->ptype_,Scheduler::instance().clock(), \
		   //curr_bucket_contents,packetsize,peak_,burst_size_);
	
	//printk(KERN_DEBUG "[mengy][in_profile] packetsize:%ld,curr_bucket:%ld",packetsize,dsshaper_my.curr_bucket_contents);	
	if (packetsize > dsshaper_my.curr_bucket_contents)
		return false;
	else {
		dsshaper_my.curr_bucket_contents -= packetsize ;
		return true ;
	}
}

void update_bucket_contents()
{
//      I'm using the token bucket implemented by Sean Murphy
        
	//double current_time = Scheduler::instance().clock() ;
	struct timespec current_time;
	getnstimeofday(&current_time);
	//struct timeval *tv;
	//void do_gettimeofday(tv);
	//u32 current_time = tv.tv_sec * 1000000 + tv.tv_nsec / 1000;
	struct timespec tmp_sub = timespec_sub(current_time,dsshaper_my.last_time);

	//u64 tmp_number = 1000000; 
	int added_bits = (tmp_sub.tv_sec * 1000000 + tmp_sub.tv_nsec /1000) *  flow_peak /1000000;  // s * bits/s
	//tmp_number = 10;
	dsshaper_my.curr_bucket_contents += (int) (added_bits * 10  + 5) /10;
	if (dsshaper_my.curr_bucket_contents > dsshaper_my.burst_size_)
		dsshaper_my.curr_bucket_contents=dsshaper_my.burst_size_ ; //unsettled how to update burst_size
	
	//printk(KERN_DEBUG "[mengy][update_bucket_contents] curr_bucket:%ld,add bits:%ld",dsshaper_my.curr_bucket_contents,added_bits);	
	dsshaper_my.last_time = current_time ;


}

void update_deqrate(struct timespec p_delay,struct timespec all_delay, int pktsize_, int pnumber_)
{
	//printk(KERN_DEBUG "pdelay:%ld.%ld,alldelay_:%ld.%ld,pktsize_:%ld,pnumber_:%ld\n",pdelay_sec,pdelay_nsec,alldelay_sec,alldelay_nsec,pktsize_,pnumber_);
	struct timespec now_;
	getnstimeofday(&now_);
	//double now_ = Scheduler::instance().clock();
	printk(KERN_DEBUG "[mengy][update_deqrate entrance][time=%ld.%ld][p_delay=%ld.%ld][all_delay=%ld.%ld][pktsize_=%d bite][pktnumber_=%d]\n",now_.tv_sec,now_.tv_nsec,p_delay.tv_sec,p_delay.tv_nsec,all_delay.tv_sec,all_delay.tv_nsec,pktsize_*8,pnumber_);
	
	


	int pri_peak_ = flow_peak;
	ntrans_ = ntrans_ + pnumber_;
	//delay_sum_ += pdelay_;
	delay_sum_ = timespec_add(delay_sum_,p_delay);
	pktsize_sum_ += pktsize_*8;

	struct timespec tmp_sub = timespec_sub(now_, checktime_); // unsettled checktime_
	if( timespec_compare(&tmp_sub,&checkInterval_) >0 ){
		int delay_instant_ = (delay_sum_.tv_sec * 1000000 + delay_sum_.tv_nsec/1000)/ntrans_; //us
		delay_avg_ = alpha_ * delay_avg_  / 100 + ( 100 - alpha_) * delay_instant_/100;//us
		
		
		
		rate_avg_ = pktsize_sum_ / (delay_sum_.tv_sec * 1000000 + delay_sum_.tv_nsec /1000) ; //bits/us
		if (switchOn_)
		{
			if( delay_avg_ > delay_optimal_ )
			{
				update_bucket_contents();
				flow_peak = delay_optimal_ * pri_peak_ / delay_avg_; //unsettled
				if (flow_peak  < beta_)
					flow_peak = beta_;
			}else{
				update_bucket_contents();
				flow_peak =  pri_peak_ + deltaIncrease_;
				if (flow_peak  > rate_avg_ * 1000000 )
					flow_peak = rate_avg_ * 1000000 ;
			}
		}else{
			flow_peak = fix_peak; //fixed rate 
		}
		ntrans_ = 0;
		pktsize_sum_ = 0;
		delay_sum_.tv_sec = 0;
		delay_sum_.tv_nsec = 0;
		checktime_ = now_;
		
	}	
	printk(KERN_DEBUG "[mengy][update_deqrate after peak ][time=%ld.%ld][rate=%ld][delay_avg=%ld][pri_peak=%ld][now_peak_=%ld]\n",now_.tv_sec,now_.tv_nsec,rate_avg_,delay_avg_,pri_peak_,flow_peak);
	
	
	
	throughput_sum_ += pktsize_;
	tmp_sub = timespec_sub(now_,checkThtime_);
	if(  timespec_compare(&tmp_sub,&checkThInterval_)>0 ){
		int throughput_avg_ = ( 8 * throughput_sum_ )/((int) (tmp_sub.tv_sec * 1000000 + tmp_sub.tv_nsec / 1000 )) * 1000;
		printk(KERN_DEBUG "[mengy][update_deqrate throughput][time=%ld.%ld][bytes=%ld][throughput=%ld Kbps]\n",now_.tv_sec,now_.tv_nsec,throughput_sum_,throughput_avg_);
		throughput_sum_ = 0;
		checkThtime_ = now_;
	}
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







