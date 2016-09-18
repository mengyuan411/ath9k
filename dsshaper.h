/*
 * Copyright (c) 2008-2011 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

//#ifndef ATH9K_H
//#define ATH9K_H

#include <linux/etherdevice.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/leds.h>
#include <linux/completion.h>
#include <linux/time.h>
#include <linux/timer.h> //for timer mengy
#include <linux/hw_random.h>

#include "common.h"
#include "debug.h"
#include "mci.h"
#include "dfs.h"

extern struct ieee80211_ops ath9k_ops;
extern int ath9k_modparam_nohwcrypt;
extern int ath9k_led_blink;
extern bool is_ath9k_unloaded;
extern int ath9k_use_chanctx;


#ifndef ATH9K_DSSHAPPER_H
#define ATH9K_DSSHAPPER_H





class DSShaper;

class DSShaperHandler : public Handler {
public:
	DSShaperHandler(DSShaper *s) : shaper_(s) {}
	void handle(Event *e);
private:
	DSShaper *shaper_;
};


class DSShaper: public Connector {
private:
	int		received_packets ;
	int		sent_packets ;
	int		shaped_packets ;
	int		dropped_packets ;
	
	PacketQueue shape_queue;
	
	int		curr_bucket_contents ;
	int		flow_id_;
	double      last_time ;
        bool        shape_packet(Packet *p) ;	
        void        schedule_packet(Packet *p) ;
        bool        in_profile(Packet *p) ;
	void        reset_counters();
	DSShaperHandler sh_;
public:
			DSShaper() ;
	void 		resume();
	void		recv(Packet *p, Handler *h) ;
	int		command(int argc, const char*const* argv) ;
	double		peak_ ;
	void		update_bucket_contents() ;
	int		burst_size_ ;
	int         max_queue_length;
} ;

//bool shape_packet(struct list_head *packet,struct ath_softc *sc, struct ath_txq *txq,bool internal,int len);
int list_length(struct list_head *head);
int timer_module(double time_delay,struct timer_list *my_timer);
void recv(int len, struct ath_softc *sc, struct ath_txq *txq, struct list_head *p, bool internal);
bool shape_packet(struct list_head *packet,struct ath_softc *sc, struct ath_txq *txq,bool internal,int len);
void schedule_packet(list_head *p,int len);
void resume();
bool in_profile(int size);
void update_bucket_contents();
/*
void schedule_packet(Packet *p);
bool in_profile(Packet *p);
void reset_counters();
void resume();
void recv(Packet *p, Handler *h);
int	 command(int argc, const char*const* argv);
void update_bucket_contents();
int list_length(struct list_head *head);
*/
struct DSShaper {
	int		received_packets ;
	int		sent_packets ;
	int		shaped_packets ;
	int		dropped_packets ;
	int		curr_bucket_contents ;
	int		flow_id_;
	double      last_time ;
	double		peak_ ;
	int		burst_size_ ;
	int         max_queue_length;
};

struct packet_msg
{
	/* data */
	struct list_head list;
	struct ath_softc *sc;
	struct ath_txq *txq;
	bool internal;
	int len;


};
	//struct list_head shape_queue;

//DSShaper()
  //{
    //received_packets = 0;
	//sent_packets = 0;
	//shaped_packets = 0;
	//dropped_packets = 0;
	//curr_bucket_contents = 0;
	//flow_id_ = 0;
	//max_queue_length= 0;
	//last_time = 0;
    //shape_queue = NULL;

  //

struct timer_list a_timer;
