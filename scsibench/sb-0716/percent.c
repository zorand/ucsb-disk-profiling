#define NUM 500

float start,ending;
float x;
int cnt[NUM];
int ro[NUM];

main(int argc, char **argv){
  int i,s=0;

  start=6000.;
  ending=10000.;
  if(argc>1) sscanf(argv[1],"%f",&start);
  if(argc>2) sscanf(argv[2],"%f",&ending);
  

  for(i=0;i<NUM;i++) cnt[i]=0,ro[i]=0;
  
  while(scanf("%f",&x)>0){
    for(i=1;i<=NUM;i++) if(x<start+ i*(ending-start)/NUM) cnt[i-1]++;
    for(i=1;i<=NUM;i++) 
      if(x<start+ i*(ending-start)/NUM && x>start+ (i-1)*(ending-start)/NUM) 
        ro[i-1]++;
    s++;
  }
  
  for(i=0;i<NUM;i++)
    printf("%f %f %f\n", 
      start+ i*(ending-start)/NUM, 1.0*cnt[i]/s, 1.0*ro[i]/s);
}
