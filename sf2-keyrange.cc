#include "iffdigest.h"
#include "sf2.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <map>
#include <set>

// SF2 Generator IDs from specification
#define SF_GEN_KEYRANGE    43
#define SF_GEN_VELRANGE    44
#define SF_GEN_OVERRIDING_ROOT_KEY 58

struct NoteRange {
  uint8_t minKey;
  uint8_t maxKey;
  uint8_t minVel;
  uint8_t maxVel;
  
  NoteRange() : minKey(0), maxKey(127), minVel(0), maxVel(127) {}
};

void extractNoteRanges(SF2Hydra& hydra) {
  std::string strName;
  
  std::cout << "{\n";
  std::cout << "  \"presets\": [\n";
  
  bool firstPreset = true;
  
  // Bank and Preset loop
  for(bankPresetMapIter_t b_i = hydra.bank_begin(); b_i != hydra.bank_end(); b_i++) {
    for(presetMapIter_t p_i = hydra.preset_begin(b_i); p_i != hydra.preset_end(b_i); p_i++) {
      const sfPresetHeader_t& ph = hydra.getPresetHeader(p_i);
      const presInfo_t& pi = hydra.getPresetInfo(p_i);
      sf2NameToStr(strName, ph.achPresetName);
      
      if (!firstPreset) {
        std::cout << ",\n";
      }
      firstPreset = false;
      
      std::cout << "    {\n";
      std::cout << "      \"bank\": " << b_i->first << ",\n";
      std::cout << "      \"preset\": " << p_i->first << ",\n";
      std::cout << "      \"name\": \"" << strName << "\",\n";
      std::cout << "      \"instruments\": [\n";
      
      bool firstInstrument = true;
      
      // Process each preset zone (bag) to find instruments
      for(uint32_t bagIdx = 0; bagIdx < pi.pbagInfoVec.size(); bagIdx++) {
        if(!pi.pbagInfoVec[bagIdx].instOrSample) {
          // Global zone - skip for now
          continue;
        }
        
        // Get the instrument for this preset zone
        uint16_t instIdx = pi.pbagInfoVec[bagIdx].instOrSampleIdx;
        const sfInst_t& sfi = hydra.getInstrument(instIdx);
        const instInfo_t& ii = hydra.getInstrumentInfo(instIdx);
        std::string instName;
        sf2NameToStr(instName, sfi.achInstName);
        
        if (!firstInstrument) {
          std::cout << ",\n";
        }
        firstInstrument = false;
        
        std::cout << "        {\n";
        std::cout << "          \"name\": \"" << instName << "\",\n";
        std::cout << "          \"zones\": [\n";
        
        bool firstZone = true;
        std::set<std::pair<uint8_t, uint8_t>> keyRanges;
        
        // Process each instrument zone (bag)
        for(uint32_t iBagIdx = 0; iBagIdx < ii.ibagInfoVec.size(); iBagIdx++) {
          NoteRange range;
          bool hasKeyRange = false;
          bool hasVelRange = false;
          
          if (!firstZone) {
            std::cout << ",\n";
          }
          firstZone = false;
          
          // Check generators for this zone
          for(auto genIdx : ii.ibagInfoVec[iBagIdx].genIdxs) {
            const sfGenList_t& gen = hydra.getInstrumentGenerator(genIdx);
            
            if(gen.sfGenOper == SF_GEN_KEYRANGE) {
              // Key range is stored as two bytes: lo key | hi key
              range.minKey = gen.genAmount & 0xFF;
              range.maxKey = (gen.genAmount >> 8) & 0xFF;
              hasKeyRange = true;
            }
            else if(gen.sfGenOper == SF_GEN_VELRANGE) {
              // Velocity range is stored as two bytes: lo vel | hi vel  
              range.minVel = gen.genAmount & 0xFF;
              range.maxVel = (gen.genAmount >> 8) & 0xFF;
              hasVelRange = true;
            }
          }
          
          // If no explicit key range, it covers all keys (0-127)
          if (!hasKeyRange) {
            range.minKey = 0;
            range.maxKey = 127;
          }
          
          // If no explicit velocity range, it covers all velocities (0-127)
          if (!hasVelRange) {
            range.minVel = 0;
            range.maxVel = 127;
          }
          
          keyRanges.insert(std::make_pair(range.minKey, range.maxKey));
          
          std::cout << "            {\n";
          std::cout << "              \"keyRange\": { \"min\": " << (int)range.minKey 
                    << ", \"max\": " << (int)range.maxKey << " },\n";
          std::cout << "              \"velRange\": { \"min\": " << (int)range.minVel 
                    << ", \"max\": " << (int)range.maxVel << " }\n";
          std::cout << "            }";
        }
        
        std::cout << "\n          ],\n";
        
        // Calculate overall range for this instrument
        if (!keyRanges.empty()) {
          auto minRange = keyRanges.begin();
          auto maxRange = keyRanges.rbegin();
          std::cout << "          \"overallKeyRange\": { \"min\": " << (int)minRange->first 
                    << ", \"max\": " << (int)maxRange->second << " }\n";
        } else {
          std::cout << "          \"overallKeyRange\": { \"min\": 0, \"max\": 127 }\n";
        }
        
        std::cout << "        }";
      }
      
      std::cout << "\n      ]\n";
      std::cout << "    }";
    }
  }
  
  std::cout << "\n  ]\n";
  std::cout << "}\n";
}

int main(int argc, char* argv[])
{
  // Check cmdline
  if(argc < 2) {
    std::cerr << "usage: " << argv[0] << " filename.sf2\n";
    std::cerr << "Extract key ranges for each preset/instrument in SF2 file (JSON output)\n";
    return -1;
  }

  // Open soundfont
  int fd = open(argv[1], O_RDONLY);
  if(fd < 0) {
    std::cerr << argv[1] << " could not be opened!\n";
    return -1;
  }

  // Parse soundfont
  struct stat stbuf;
  fstat(fd, &stbuf);
  char* data = (char*)mmap(0, stbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
  
  // Redirect stdout to /dev/null to suppress library debug output during parsing
  std::streambuf* orig = std::cout.rdbuf();
  std::ofstream devnull("/dev/null");
  std::cout.rdbuf(devnull.rdbuf());
  
  IFFDigest iff(data, stbuf.st_size);
  int retval = -1;

  // Analyze SF2 file
  SF2File sfFile;
  if(sfFile.Analyse(&iff)) {
    // Restore stdout for JSON output
    std::cout.rdbuf(orig);
    
    SF2Hydra& hydra = sfFile.getHydra();
    extractNoteRanges(hydra);
    retval = 0;
  } else {
    // Restore stdout before error
    std::cout.rdbuf(orig);
    std::cerr << "Failed to analyze SF2 file\n";
  }

  munmap(data, stbuf.st_size);
  close(fd);
  return retval;
}