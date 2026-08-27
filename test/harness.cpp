/*!
 * @file harness.cpp
 * Host-runnable ASan/UBSan harness for the DFRobot_C4001 parser functions
 * (analysisData / analysisResponse). Build with sanitizers enabled:
 *
 *   g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined \
 *       -Idir/test/include src/DFRobot_C4001.cpp dir/test/harness.cpp -o out
 */
#include "DFRobot_C4001.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

class ParserProbe : public DFRobot_C4001 {
public:
  sAllData_t      parseData(uint8_t *d, uint8_t l) { return analysisData(d, l); }
  sResponseData_t parseResponse(uint8_t *d, uint8_t l, uint8_t c) {
    return analysisResponse(d, l, c);
  }
protected:
  virtual void     writeReg(uint8_t, uint8_t *, uint8_t) {}
  virtual int16_t  readReg(uint8_t, uint8_t *, uint8_t) { return 0; }
};

TwoWire Wire;

static int failures = 0;
static int checks   = 0;

static void expect(const char *name, bool cond)
{
  checks++;
  if(!cond){
    failures++;
    std::printf("  FAIL: %s\n", name);
  }
}

static void feedResponse(ParserProbe &p, const char *lit, uint8_t count)
{
  unsigned char buf[128];
  unsigned n = (unsigned)std::strlen(lit);
  for(unsigned i = 0; i < n; i++) buf[i] = (unsigned char)lit[i];
  sResponseData_t r = p.parseResponse(buf, (uint8_t)n, count);
  (void)r;
}

int main(void)
{
  ParserProbe p;

  /* ---- $DFHPD valid frames ------------------------------------------- */
  std::printf("-- $DFHPD valid --\n");
  {
    // Frame layout: "$DFHPD" (6 chars) then a separator byte, then the exist
    // flag at frame start + 7 (matches the vendor Python parser).
    const char f[] = "$DFHPD,0*";
    sAllData_t d = p.parseData((uint8_t*)f, (uint8_t)std::strlen(f));
    expect("HPD exist=0 defaults", d.exist == 0 &&
       d.sta.workStatus == 1 && d.sta.workMode == 0 && d.sta.initStatus == 1);
  }
  {
    const char f[] = "$DFHPD,1*";   // exist byte at frame start+7 == '1'
    sAllData_t d = p.parseData((uint8_t*)f, (uint8_t)std::strlen(f));
    expect("HPD exist=1", d.exist == 1);
  }
  /* $DFHPD truncated right after the type (missing the exist byte) -> no OOB */
  {
    const char f[] = "$DFH";
    sAllData_t d = p.parseData((uint8_t*)f, (uint8_t)std::strlen(f));
    expect("HPD truncated -> zeroed", d.exist == 0 && d.sta.workStatus == 0);
  }

  /* ---- $DFDMD valid ---- --------------------------------------------- */
  std::printf("-- $DFDMD valid --\n");
  {
    // $DFDMD,<num>,<p2>,<range>,<speed>,<energy>,...  ->  parts[1]=num,
    // parts[3]=range, parts[4]=speed, parts[5]=energy (raw on-wire values,
    // matching the vendor Python reference).
    const char f[] = "$DFDMD,1,2,325,1050,42,7,8*";
    sAllData_t d = p.parseData((uint8_t*)f, (uint8_t)std::strlen(f));
    expect("DMD full frame mode", d.sta.workStatus == 1 && d.sta.workMode == 1 && d.sta.initStatus == 1);
    expect("DMD number", d.target.number == 1);
    expect("DMD range", (int)d.target.range == 325);
    expect("DMD speed", (int)d.target.speed == 1050);
    expect("DMD energy", (int)d.target.energy == 42);
  }

  /* ---- truncated $DFDMD (0..7 fields): zeroed struct, no OOB ---------- */
  std::printf("-- truncated $DFDMD 0..7 fields --\n");
  for(int nfields = 0; nfields <= 7; nfields++){
    char f[64];
    std::strcpy(f, "$DFDMD");
    for(int k = 0; k < nfields; k++){
      std::strcat(f, ",");
      std::strcat(f, "1");
    }
    sAllData_t d = p.parseData((uint8_t*)f, (uint8_t)std::strlen(f));
    // a partial frame must not touch parts[n] past what was parsed; the
    // struct must be all-zero rather than reading garbage pointers.
    if(nfields < 6){
      expect("truncated DMD leaves target zero",
        d.target.number == 0 && d.target.range == 0.f &&
        d.target.speed == 0.f && d.target.energy == 0u);
    }
  }

  /* ---- leading garbage before DPDP frame ----------------------------- */
  std::printf("-- leading garbage --\n");
  {
    const char f[] = "qwe~@!$DFDMD,2,2,500,0,7,0,1*";
    sAllData_t d = p.parseData((uint8_t*)f, (uint8_t)std::strlen(f));
    expect("garbage -> number", d.target.number == 2);
    expect("garbage -> range", (int)d.target.range == 500);
  }
  {
    /* trash with no $ at all */
    const char f[] = "no dollar here 1234";
    sAllData_t d = p.parseData((uint8_t*)f, (uint8_t)std::strlen(f));
    expect("no $ -> zeroed", d.exist == 0 && d.target.number == 0);
  }

  /* ---- Response spacing ---- ------------------------------------------ */
  std::printf("-- response fields 0/1/3/8 --\n");
  {
    const char f[] = "Response";                       /* 0 fields */
    sResponseData_t r = p.parseResponse((uint8_t*)f, (uint8_t)std::strlen(f), 1);
    expect("0 fields status", r.status == true);
    expect("0 fields zeroed", r.response1 == 0.f && r.response2 == 0.f && r.response3 == 0.f);
  }
  {
    const char f[] = "Response 7";                     /* 1 field */
    sResponseData_t r = p.parseResponse((uint8_t*)f, (uint8_t)std::strlen(f), 1);
    expect("1 field r1", r.response1 == 7.f);
    expect("1 field r2 zero", r.response2 == 0.f);
  }
  {
    const char f[] = "Response 1 2 3";                 /* 3 fields */
    sResponseData_t r = p.parseResponse((uint8_t*)f, (uint8_t)std::strlen(f), 3);
    expect("3 fields r1..r3", r.response1 == 1.f && r.response2 == 2.f && r.response3 == 3.f);
  }
  {
    const char f[] = "Response 1 2 3 4 5 6 7 8 9";     /* 8 fields: must not overflow */
    sResponseData_t r = p.parseResponse((uint8_t*)f, (uint8_t)std::strlen(f), 3);
    expect("8 fields first3", r.response1 == 1.f && r.response2 == 2.f && r.response3 == 3.f);
  }

  /* ---- boundary / truncated frames ---- -------------------------------- */
  std::printf("-- boundary frames --\n");
  feedResponse(p, "", 0);          // empty
  feedResponse(p, "R", 1);
  feedResponse(p, "Re", 1);
  feedResponse(p, "Res", 3);         // token at the very end
  feedResponse(p, "xxRes<>", 7);     // 's' one byte before end
  feedResponse(p, "xxRes", 5);       // token runs exactly to boundary
  feedResponse(p, "   Res 1 2", 2);  // token after leading spaces
  feedResponse(p, "Response ", 1);   // trailing single space

  std::printf("\n=== %d checks, %d failures ===\n", checks, failures);
  return failures ? 1 : 0;
}