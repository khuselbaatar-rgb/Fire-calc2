#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <cmath>
#include <algorithm>

static double erf_approx(double x) {
    constexpr double a1=0.254829592,a2=-0.284496736,a3=1.421413741;
    constexpr double a4=-1.453152027,a5=1.061405429,p=0.3275911;
    int s=(x<0)?-1:1; x=std::abs(x);
    double tt=1.0/(1.0+p*x);
    double y=1.0-(((((a5*tt+a4)*tt)+a3)*tt+a2)*tt+a1)*tt*std::exp(-x*x);
    return s*y;
}
static double interp(double x,const std::vector<std::pair<double,double>>&pts){
    if(x<=pts.front().first)return pts.front().second;
    for(std::size_t i=1;i<pts.size();++i)
        if(x<=pts[i].first){double x0=pts[i-1].first,y0=pts[i-1].second,x1=pts[i].first,y1=pts[i].second;return y0+(y1-y0)*(x-x0)/(x1-x0);}
    return pts.back().second;
}
static double gammaSteel(double t){
    static const std::vector<std::pair<double,double>>pts={{20,1.0},{300,0.97},{400,0.85},{500,0.544},{600,0.37},{700,0.22},{800,0.12},{900,0.06},{1000,0.03}};
    return interp(t,pts);
}
static double phiByLambda(double lam){
    static const std::vector<std::pair<double,double>>pts={{8,0.98},{10,0.973},{11.5,0.965},{15,0.95},{20,0.90},{30,0.80},{40,0.70},{50,0.60}};
    return interp(lam,pts);
}
static std::map<std::string,double> readInput(){
    std::map<std::string,double> in; std::string line;
    while(std::getline(std::cin,line)){
        if(line.empty())continue;
        auto eq=line.find('='); if(eq==std::string::npos)continue;
        try{in[line.substr(0,eq)]=std::stod(line.substr(eq+1));}catch(...){}
    }
    return in;
}
static std::string num(double v,int prec=10){
    if(!std::isfinite(v))return"null";
    std::ostringstream os;os<<std::setprecision(prec)<<v;return os.str();
}
static double get(const std::map<std::string,double>&m,const std::string&k,double def=0.0){
    auto it=m.find(k);return(it==m.end())?def:it->second;
}

// ── Халалтын чиглэл ──────────────────────────────────────────────
// CORNER     : As1 булан    → Θx·Θy  (хоёр граниас)
// BOTTOM_FACE: As2 доод/дээд → Θy    (доод граниас a2)
// SIDE_FACE  : As3 хажуу    → Θx    (хажуу граниас a3)
struct RebarHeat{double ts;double g;};
enum class HD{CORNER,BOTTOM,SIDE};

static RebarHeat rebarHeat(double a,double b,double h,double t0,double root,double kbS,HD dir){
    RebarHeat rh{t0,1.0};
    if(root<=0.0)return rh;
    auto c01=[](double v){return std::max(0.0,std::min(1.0,v));};
    double theta=1.0;
    if(dir==HD::CORNER){
        double tx=c01(erf_approx((kbS+a)/root)+erf_approx((kbS+b-a)/root)-1.0);
        double ty=c01(erf_approx((kbS+a)/root)+erf_approx((kbS+h-a)/root)-1.0);
        theta=tx*ty;
    }else if(dir==HD::BOTTOM){
        // Доод граниас: Y чиглэл
        theta=c01(erf_approx((kbS+a)/root)+erf_approx((kbS+h-a)/root)-1.0);
    }else{
        // Хажуу граниас: X чиглэл
        theta=c01(erf_approx((kbS+a)/root)+erf_approx((kbS+b-a)/root)-1.0);
    }
    rh.ts=1250.0-(1250.0-t0)*theta;
    rh.g=gammaSteel(rh.ts);
    return rh;
}

struct SR{double tau,root,delta,ts1,ts2,ts3,g1,g2,g3,Nu;bool ok;};

static SR step(double tau,double b,double h,double a1,double a2,double a3,
               double Rbn,double Rsn,double As1,double As2,double As3,
               double t0,double Np,double phi,double aRed,double kbS){
    SR r{};
    r.tau=tau;
    r.root=(tau==0.0)?0.0:2.0*std::sqrt(std::max(aRed,0.0)*tau*60.0);
    r.ts1=r.ts2=r.ts3=t0; r.g1=r.g2=r.g3=1.0; r.delta=0.0;
    if(tau>0.0){
        // As1: булан → CORNER (X·Y)
        if(As1>0){auto rh=rebarHeat(a1,b,h,t0,r.root,kbS,HD::CORNER);     r.ts1=rh.ts;r.g1=rh.g;}
        // As2: доод/дээд грань → BOTTOM (Y)
        if(As2>0){auto rh=rebarHeat(a2,b,h,t0,r.root,kbS,HD::BOTTOM);     r.ts2=rh.ts;r.g2=rh.g;}
        // As3: зүүн/баруун грань → SIDE (X)
        if(As3>0){auto rh=rebarHeat(a3,b,h,t0,r.root,kbS,HD::SIDE);       r.ts3=rh.ts;r.g3=rh.g;}
        r.delta=std::max(0.0,std::min(std::min(b,h)/2.0-1.0,0.3807*r.root-kbS));
    }
    double bb=std::max(1.0,b-2.0*r.delta),hh=std::max(1.0,h-2.0*r.delta);
    r.Nu=phi*(Rbn*bb*hh + r.g1*Rsn*As1 + r.g2*Rsn*As2 + r.g3*Rsn*As3)*1.0e-3;
    r.ok=(r.Nu>=Np);
    return r;
}

int main(){
    auto in=readInput();
    double b=get(in,"b"),h=get(in,"h"),H0=get(in,"H0"),kL=get(in,"kL");
    double a1=get(in,"a1",get(in,"c1",50.0));
    double a2=get(in,"a2",get(in,"c1",50.0));
    double a3=get(in,"a3",get(in,"c1",50.0));
    double Rbn=get(in,"Rbn"),Rsn=get(in,"Rsn"),rho=get(in,"rho");
    double W=get(in,"W"),tb=get(in,"tb"),t0=get(in,"t0");
    double As1=get(in,"As1"),As2=get(in,"As2"),As3=get(in,"As3",0.0);
    double Np=get(in,"Np"),stepT=get(in,"step",30.0),tmax=get(in,"tmax",500.0);
    bool hasPhi=in.count("phiManual")>0;
    double lambdaTem=1.14-0.00055*tb,cTem=710.0+0.84*tb;
    double aRed=(lambdaTem/((cTem+50.0*W)*rho))*1.0e6;
    double kbS=37.2*std::sqrt(std::max(aRed,0.0));
    double l0=kL*H0,lambda=l0/std::min(b,h);
    double phi=hasPhi?in["phiManual"]:phiByLambda(lambda);
    double AsTot=As1+As2+As3,N0=phi*(Rbn*b*h+Rsn*AsTot)*1.0e-3;

    std::vector<double> times;
    for(double tau=0;tau<=tmax+1e-9;tau+=stepT)times.push_back(tau);
    if(times.empty()||std::abs(times.back()-tmax)>1e-9)times.push_back(tmax);

    std::ostringstream out;
    out<<"{"
       <<"\"lambdaTem\":"<<num(lambdaTem)<<",\"cTem\":"<<num(cTem)<<",\"aRed\":"<<num(aRed)<<",\"kbS\":"<<num(kbS)<<","
       <<"\"l0\":"<<num(l0)<<",\"lambda\":"<<num(lambda)<<",\"phi\":"<<num(phi)<<",\"phiManual\":"<<(hasPhi?"true":"false")<<","
       <<"\"As1\":"<<num(As1)<<",\"As2\":"<<num(As2)<<",\"As3\":"<<num(As3)<<",\"AsTot\":"<<num(AsTot)<<","
       <<"\"N0\":"<<num(N0)<<",\"Np\":"<<num(Np)<<",\"N0pass\":"<<(N0>=Np?"true":"false")<<","
       <<"\"rows\":[";

    bool first=true; int failIdx=-1; std::vector<double> Nus;
    for(std::size_t i=0;i<times.size();++i){
        auto r=step(times[i],b,h,a1,a2,a3,Rbn,Rsn,As1,As2,As3,t0,Np,phi,aRed,kbS);
        if(!r.ok&&failIdx<0)failIdx=(int)i;
        Nus.push_back(r.Nu);
        if(!first)out<<","; first=false;
        out<<"{"<<"\"tau\":"<<num(r.tau)<<",\"root\":"<<num(r.root)<<",\"delta\":"<<num(r.delta)<<","
           <<"\"ts1\":"<<num(r.ts1)<<",\"ts2\":"<<num(r.ts2)<<",\"ts3\":"<<num(r.ts3)<<","
           <<"\"g1\":"<<num(r.g1)<<",\"g2\":"<<num(r.g2)<<",\"g3\":"<<num(r.g3)<<","
           <<"\"Nu\":"<<num(r.Nu)<<",\"ok\":"<<(r.ok?"true":"false")<<"}";
    }
    out<<"],\"chartRows\":[";

    bool fc=true; double pTau=0,pNu=0; bool fp=true,hCross=false; double cTau=-1;
    for(double tau=0.0;tau<=tmax+1e-9;tau+=1.0){
        auto r=step(tau,b,h,a1,a2,a3,Rbn,Rsn,As1,As2,As3,t0,Np,phi,aRed,kbS);
        if(!fp&&!hCross&&pNu>=Np&&r.Nu<Np){
            cTau=(r.Nu!=pNu)?pTau+(Np-pNu)*(tau-pTau)/(r.Nu-pNu):tau; hCross=true;
        }
        pTau=tau; pNu=r.Nu; fp=false;
        if(!fc)out<<","; fc=false;
        out<<"{"<<"\"tau\":"<<num(r.tau)<<",\"Nu\":"<<num(r.Nu)<<","
           <<"\"ts1\":"<<num(r.ts1)<<",\"ts2\":"<<num(r.ts2)<<",\"ts3\":"<<num(r.ts3)<<","
           <<"\"delta\":"<<num(r.delta)<<",\"ok\":"<<(r.ok?"true":"false")<<"}";
    }
    out<<"],";

    std::string verdict="more"; double tExact=-1.0;
    if(failIdx==0){verdict="zero";}
    else if(hCross){tExact=cTau;verdict="approx";}
    else if(failIdx>0){
        double pT=times[failIdx-1],pN=Nus[failIdx-1],cT=times[failIdx],cN=Nus[failIdx];
        if(cN!=pN)tExact=pT+(Np-pN)*(cT-pT)/(cN-pN); verdict="approx";
    }
    out<<"\"verdict\":\""<<verdict<<"\",\"tExact\":"<<num(tExact)<<",\"tauLast\":"<<num(times.back())<<"}";
    std::cout<<out.str()<<std::endl;
    return 0;

