/* Track V: NON-NORMATIVE reference C observation only.
 * Compile ONLY against pinned unmodified DaveGamble/cJSON @
 * 6d9f2443ab071f86e5d9b43025a40929ec41c46c; see README.md.
 * No NewLang semantic proof or North Star oracle modification. */
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define LIMIT 512
typedef struct { void *p; size_t size; unsigned id; int live; int freed; } Rec;
static Rec rec[LIMIT];
static unsigned used, trials, fail_call, sequence, errors;
static const char *scenario;
static int perturb_edge, perturb_free;

static Rec *record_live(const void *p) {
    unsigned i;
    for (i=0;i<used;i++) if (rec[i].p==p && rec[i].live) return &rec[i];
    return NULL;
}
static unsigned id(const void *p) { Rec *r=record_live(p); return r?r->id:0; }
static int live(const void *p) { return record_live(p)!=NULL; }
static unsigned frees_for(unsigned identity) { return identity && identity<=used?(unsigned)rec[identity-1].freed:0; }
static unsigned outstanding(void) { unsigned i,n=0;for(i=0;i<used;i++)n+=(unsigned)rec[i].live;return n; }

static void *observe_alloc(size_t n) {
    void *p;
    trials++;
    if (trials==fail_call) {
        printf("{\"case\":\"%s\",\"event\":\"malloc_NULL\",\"seq\":%u,\"trial\":%u,\"size\":%zu}\n",scenario,++sequence,trials,n);
        return NULL;
    }
    p=malloc(n);
    if (p==NULL) {
        printf("{\"case\":\"%s\",\"event\":\"malloc_OS_NULL\",\"seq\":%u,\"trial\":%u}\n",scenario,++sequence,trials);
        return NULL;
    }
    if (used==LIMIT) { fprintf(stderr,"observer capacity exceeded\n");exit(91); }
    rec[used].p=p;rec[used].size=n;rec[used].id=used+1;rec[used].live=1;rec[used].freed=0;used++;
    printf("{\"case\":\"%s\",\"event\":\"malloc\",\"seq\":%u,\"id\":%u,\"trial\":%u,\"addr\":\"%p\",\"size\":%zu,\"class\":\"%s\"}\n",scenario,++sequence,used,trials,p,n,n==sizeof(cJSON)?"node_candidate":"other_size");
    return p;
}
static void observe_free(void *p) {
    Rec *r;
    if (!p) { free(p);return; }
    r=record_live(p);
    if (!r) {
        fprintf(stderr,"UNKNOWN_OR_DUPLICATE_FREE addr=%p case=%s\n",p,scenario);
        /* No unsafe second free. Fatal instrumentation failure. */
        exit(92);
    }
    printf("{\"case\":\"%s\",\"event\":\"free\",\"seq\":%u,\"id\":%u,\"addr\":\"%p\"}\n",scenario,++sequence,r->id,p);
    r->live=0;r->freed++;
    free(p);
}
static void expect(int condition,const char *why) {
    if(!condition){fprintf(stderr,"CHECK_FAILED case=%s check=%s\n",scenario,why);errors++;}
}
static void snapshot(const char *when,const cJSON *n) {
    Rec *r=record_live(n);
    if (!r) {fprintf(stderr,"BAD_SNAPSHOT after-free or unknown, case=%s\n",scenario);errors++;return;}
    printf("{\"case\":\"%s\",\"event\":\"edge\",\"seq\":%u,\"when\":\"%s\",\"id\":%u,\"addr\":\"%p\",\"next\":%u,\"prev\":%u,\"child\":%u,\"type\":%d,\"is_reference\":%d,\"valuestring\":%u,\"string\":%u}\n",scenario,++sequence,when,r->id,(void*)n,id(n->next),id(n->prev),id(n->child),n->type,!!(n->type & cJSON_IsReference),id(n->valuestring),id(n->string));
}
static void reset(const char *name) {
    memset(rec,0,sizeof rec);used=trials=fail_call=sequence=errors=0;
    scenario=name;
    cJSON_InitHooks(NULL);
    {cJSON_Hooks hooks={observe_alloc,observe_free};cJSON_InitHooks(&hooks);}
    printf("{\"case\":\"%s\",\"event\":\"begin\",\"sizeof_cJSON\":%zu}\n",scenario,sizeof(cJSON));
}
static void finish(void) {
    expect(outstanding()==0,"all tracked allocations freed (except explicitly classified suspected upstream leak)");
    printf("{\"case\":\"%s\",\"event\":\"finish\",\"trials\":%u,\"allocations\":%u,\"outstanding\":%u,\"errors\":%u}\n",scenario,trials,used,outstanding(),errors);
    cJSON_InitHooks(NULL);
}

static void detach_interior(void) {
    cJSON *from,*to,*a,*b,*c,*d;
    unsigned ida,idb,idc,idfrom,idto;
    reset("detach_interior");
    from=cJSON_CreateArray();a=cJSON_CreateNumber(11);b=cJSON_CreateNumber(22);c=cJSON_CreateNumber(33);
    if (!from||!a||!b||!c){expect(0,"setup allocated");return;}
    ida=id(a);idb=id(b);idc=id(c);idfrom=id(from);
    expect(cJSON_AddItemToArray(from,a),"add a");expect(cJSON_AddItemToArray(from,b),"add b");expect(cJSON_AddItemToArray(from,c),"add c");
    snapshot("before_from",from);snapshot("before_a",a);snapshot("before_b",b);snapshot("before_c",c);
    expect(from->child==a && a->next==b && b->next==c && a->prev==c && c->prev==b && !c->next,"3-node bidirectional/head-tail layout");
    d=cJSON_DetachItemViaPointer(from,b);
    expect(d==b,"same detached physical node");
    snapshot("after_from",from);snapshot("after_a",a);snapshot("detached_b",b);snapshot("after_c",c);
    expect(from->child==a && a->next==c && c->prev==a && a->prev==c,"neighbor, head, tail repair");
    expect(b->next==NULL && b->prev==NULL && live(b) && frees_for(idb)==0,"detached node remains live, disconnected, no free");
    to=cJSON_CreateArray();if(!to){expect(0,"recipient allocated");return;}idto=id(to);
    expect(cJSON_AddItemToArray(to,b),"reattach to recipient");
    snapshot("recipient",to);snapshot("recipient_b",b);
    expect(to->child==b && b->prev==b && b->next==NULL,"recipient now reaches original b");
    cJSON_Delete(from);
    expect(frees_for(ida)==1&&frees_for(idc)==1&&frees_for(idfrom)==1&&live(b),"old parent deletes only remaining nodes");
    snapshot("b_still_live_after_old_parent_delete",b);
    cJSON_Delete(to);
    expect(frees_for(idb)==1&&frees_for(idto)==1,"recipient deletes detached b exactly once");
    /* Deliberately wrong *expectation* (no graph mutation): must be detected. */
    if(perturb_edge){int false_expected=(idb==ida);expect(false_expected,"PERTURBED expected detached ID equals a");}
    finish();
}
static void replace_nodes(void) {
    cJSON *parent,*old_head,*old_nonhead,*new_head,*new_nonhead;
    unsigned ioh,ion,inh,inn,ip;
    reset("replace_nodes");
    parent=cJSON_CreateObject();old_head=cJSON_CreateNumber(1);old_nonhead=cJSON_CreateNumber(2);
    new_head=cJSON_CreateNumber(7);new_nonhead=cJSON_CreateNumber(8);
    if(!parent||!old_head||!old_nonhead||!new_head||!new_nonhead){expect(0,"setup allocated");return;}
    ioh=id(old_head);ion=id(old_nonhead);inh=id(new_head);inn=id(new_nonhead);ip=id(parent);
    expect(cJSON_AddItemToObject(parent,"first",old_head),"own head with copied key");
    expect(cJSON_AddItemToObject(parent,"second",old_nonhead),"own sibling with copied key");
    snapshot("before_parent",parent);snapshot("before_old_head",old_head);snapshot("before_old_nonhead",old_nonhead);
    expect(cJSON_ReplaceItemInObject(parent,"second",new_nonhead),"non-head replace");
    expect(frees_for(ion)==1 && live(new_nonhead),"old non-head freed, new still live");
    snapshot("after_nonhead_parent",parent);snapshot("after_nonhead_new",new_nonhead);
    expect(parent->child==old_head && old_head->next==new_nonhead && new_nonhead->prev==old_head && new_nonhead->next==NULL && old_head->prev==new_nonhead,"replace non-head physical links");
    expect(new_nonhead->string && strcmp(new_nonhead->string,"second")==0,"new key retained");
    expect(cJSON_ReplaceItemInObject(parent,"first",new_head),"head replace");
    expect(frees_for(ioh)==1&&parent->child==new_head&&new_head->next==new_nonhead&&new_nonhead->prev==new_head,"head replace and old head freed");
    snapshot("after_head_parent",parent);snapshot("after_head_new",new_head);
    cJSON_Delete(parent);
    expect(frees_for(inh)==1&&frees_for(inn)==1&&frees_for(ip)==1,"new node identities freed by owning container");
    finish();
}
static void reference_wrapper(void) {
    cJSON *owner,*target,*view,*wrapper;void *str;
    unsigned io,it,iv,iw,is;
    reset("reference_wrapper");
    owner=cJSON_CreateArray();target=cJSON_CreateString("borrowed-value");view=cJSON_CreateArray();
    if(!owner||!target||!view){expect(0,"setup allocated");return;}
    io=id(owner);it=id(target);iv=id(view);str=target->valuestring;is=id(str);
    expect(cJSON_AddItemToArray(owner,target),"own target");
    expect(cJSON_AddItemReferenceToArray(view,target),"create and insert separate reference wrapper");
    wrapper=view->child;if(!wrapper){expect(0,"wrapper exists");return;}iw=id(wrapper);
    snapshot("owner",owner);snapshot("target",target);snapshot("view",view);snapshot("wrapper",wrapper);
    expect(wrapper!=target && (wrapper->type & cJSON_IsReference)!=0 && wrapper->valuestring==str,"wrapper is independent malloc, borrows target string");
    cJSON_Delete(view);
    expect(frees_for(iv)==1&&frees_for(iw)==1&&live(target)&&live(str)&&frees_for(it)==0&&frees_for(is)==0,"reference wrapper freed, borrowed target and value survive");
    snapshot("target_after_view_delete",target);
    cJSON_Delete(owner);
    expect(frees_for(io)==1&&frees_for(it)==1&&frees_for(is)==1,"actual owner frees target and valuestring once");
    finish();
}
static void recursive_delete(void) {
    cJSON *root,*array,*object,*leaf,*sibling,*outside,*external,*ref;
    unsigned ids[8], k;
    reset("recursive_delete");
    root=cJSON_CreateObject();array=cJSON_CreateArray();object=cJSON_CreateObject();
    leaf=cJSON_CreateString("deep");sibling=cJSON_CreateNumber(4);
    outside=cJSON_CreateArray();external=cJSON_CreateNumber(87);
    if(!root||!array||!object||!leaf||!sibling||!outside||!external){expect(0,"setup allocated");return;}
    ids[0]=id(root);ids[1]=id(array);ids[2]=id(object);ids[3]=id(leaf);ids[4]=id(sibling);ids[5]=id(outside);ids[6]=id(external);
    expect(cJSON_AddItemToObject(root,"array",array),"root owns array");
    expect(cJSON_AddItemToArray(array,object),"array owns object");
    expect(cJSON_AddItemToObject(object,"leaf",leaf),"object owns leaf");
    expect(cJSON_AddItemToArray(array,sibling),"array owns sibling");
    expect(cJSON_AddItemToArray(outside,external),"external owner");
    expect(cJSON_AddItemReferenceToObject(object,"borrowed",external),"reference wrapper to external");
    ref=cJSON_GetObjectItem(object,"borrowed");if(!ref){expect(0,"reference exists");return;}ids[7]=id(ref);
    snapshot("before_root",root);snapshot("before_array",array);snapshot("before_object",object);snapshot("before_leaf",leaf);snapshot("before_sibling",sibling);snapshot("reference",ref);snapshot("external",external);
    expect((ref->type & cJSON_IsReference)!=0 && ref->valuedouble==external->valuedouble,"reference wrapper preserves borrowed scalar");
    cJSON_Delete(root);
    for(k=0;k<5;k++) expect(frees_for(ids[k])==1,"each recursively owned node freed once");
    expect(frees_for(ids[7])==1&&live(external)&&frees_for(ids[6])==0,"wrapper freed, independent external target retained");
    snapshot("external_after_recursive_delete",external);
    cJSON_Delete(outside);
    expect(frees_for(ids[5])==1&&frees_for(ids[6])==1,"external owner eventually frees own target");
    finish();
}
static void allocation_failures(void) {
    cJSON *parent,*node,*owner,*refnode;
    unsigned ip,in,io,it, before;
    reset("fail_node_create");fail_call=1;
    node=cJSON_CreateNumber(15);
    expect(node==NULL&&trials==1&&outstanding()==0,"failed node creation returns NULL without grant/free");
    finish();

    reset("fail_object_key");
    parent=cJSON_CreateObject();node=cJSON_CreateNumber(15);
    if(!parent||!node){expect(0,"setup allocated");return;}
    ip=id(parent);in=id(node);before=trials;fail_call=before+1;
    expect(!cJSON_AddItemToObject(parent,"key",node),"key allocation failure returns false");
    expect(parent->child==NULL && live(node) && frees_for(in)==0,"caller retains unlinked node after failed add");
    snapshot("key_failure_parent",parent);snapshot("key_failure_node",node);
    fail_call=0;cJSON_Delete(node);cJSON_Delete(parent);
    expect(frees_for(in)==1&&frees_for(ip)==1,"caller cleans up after failure");
    finish();

    reset("fail_wrapper_create");
    owner=cJSON_CreateArray();refnode=cJSON_CreateNumber(4);
    if(!owner||!refnode){expect(0,"setup allocated");return;}
    io=id(owner);it=id(refnode);expect(cJSON_AddItemToArray(owner,refnode),"own refnode");
    before=trials;fail_call=before+1;
    expect(!cJSON_AddItemReferenceToArray(owner,refnode),"wrapper allocation failed and returned false");
    expect(owner->child==refnode && refnode->next==NULL && live(refnode),"existing graph untouched on wrapper OOM");
    fail_call=0;cJSON_Delete(owner);
    expect(frees_for(io)==1&&frees_for(it)==1,"existing owner cleans up correctly");
    finish();

    /* Source-derived cJSON OOM issue must be isolated and classified, not
       silently 'fixed' by the observer. Enable separately: --probe-orphan.
       Here a newly allocated reference wrapper can become unreachable after
       its object key strdup fails. This is a leak trace, not a passing case. */
}
static void orphan_probe(void) {
    cJSON *owner,*item;unsigned n,ip,it;
    reset("orphan_wrapper_key_OOM");
    owner=cJSON_CreateObject();item=cJSON_CreateNumber(23);
    if(!owner||!item){expect(0,"setup allocated");return;}
    ip=id(owner);it=id(item);
    n=trials;fail_call=n+2;
    expect(!cJSON_AddItemReferenceToObject(owner,"key",item),"reference key allocation fails");
    expect(owner->child==NULL,"wrapper not linked to object");
    fail_call=0;cJSON_Delete(item);cJSON_Delete(owner);
    expect(frees_for(it)==1&&frees_for(ip)==1,"reachable objects freed");
    printf("{\"case\":\"%s\",\"event\":\"potential_upstream_leak\",\"outstanding\":%u}\n",scenario,outstanding());
    /* Intentionally NOT hiding leak or manufacturing a free: process exits
       with distinct result code. Do not run this as part of passing suite. */
}
int main(int argc,char **argv) {
    int orphan=0;int i;
    for(i=1;i<argc;i++){
        if(strcmp(argv[i],"--perturb-edge")==0)perturb_edge=1;
        else if(strcmp(argv[i],"--perturb-free")==0)perturb_free=1;
        else if(strcmp(argv[i],"--probe-orphan")==0)orphan=1;
        else{fprintf(stderr,"unknown option %s\n",argv[i]);return 2;}
    }
    if(orphan){orphan_probe();return outstanding()==1?4:5;}
    detach_interior();
    if(perturb_free){expect(frees_for(2)==0,"PERTURBED expected allocated b was never freed");}
    {unsigned subtotal=errors; if(subtotal)fprintf(stderr,"OBSERVER_FAILURES scenario=%s count=%u\n",scenario,subtotal);
     replace_nodes();subtotal+=errors;reference_wrapper();subtotal+=errors;recursive_delete();subtotal+=errors;allocation_failures();subtotal+=errors;
     if(subtotal){fprintf(stderr,"DETECTED_NEGATIVE_EXPECTATION_OR_ERROR count=%u\n",subtotal);return 3;}}
    puts("REFERENCE_C_BASELINE_ALL_CHECKS_PASS");return 0;
}
