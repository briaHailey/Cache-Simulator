#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <sstream>
#include <cmath>
#include <vector>

#define NUMMEMORY 65536 /* maximum number of words in memory */
#define MAXCACHEBLOCKS 256
#define MAXLINELENGTH 1000


using namespace std;

enum actionType {
    cacheToProcessor,
    processorToCache,
    memoryToCache,
    cacheToMemory,
    cacheToNowhere
};

/*
 * Log the specifics of each cache action.
 *
 * address is the starting word address of the range of data being transferred.
 * size is the size of the range of data being transferred.
 * type specifies the source and destination of the data being transferred.
 *     cacheToProcessor: reading data from the cache to the processor
 *     processorToCache: writing data from the processor to the cache
 *     memoryToCache: reading data from the memory to the cache
 *     cacheToMemory: evicting cache data by writing it to the memory
 *     cacheToNowhere: evicting cache data by throwing it away
 */

void
printAction(int address, int size, enum actionType type)
{
    printf("@@@ transferring word [%d-%d] ", address, address + size - 1);
    if (type == cacheToProcessor) {
        printf("from the cache to the processor\n");
    } else if (type == processorToCache) {
        printf("from the processor to the cache\n");
    } else if (type == memoryToCache) {
        printf("from the memory to the cache\n");
    } else if (type == cacheToMemory) {
        printf("from the cache to the memory\n");
    } else if (type == cacheToNowhere) {
        printf("from the cache to nowhere\n");
    }
}

struct memoryUnit {
    int blockNum;
    int setNum;
    int data;
};

struct memoryBlock {
    int* data;
    int setNum;
};

// Cache block struct
struct cacheBlock {
    bool validBit;
    bool dirtyBit;
    string tag;
    int* cacheBlockDataArray;
    int setIndex;
    int timeSinceLastUse;
};

int vectorToInt(const std::vector<bool>& binaryVector);

string vectorToString(const vector<bool> &v);

vector<bool> stringToVector(const string &s);

vector<bool> intToVector(const int num);

int findLRU(cacheBlock* cacheBlockArray, int numCacheBlock, int setNum);

void ShowPrimaryMemory(memoryUnit*);

// int stringToInt(const string &s);


int main(int argc, char *argv[]) {

    // UNCOMMENT BEFORE SUBMISSION
//    // Load file
//    ifstream inputFile(argv[1]);
//    if (!inputFile.is_open()) {
//        cerr << "Error: Unable to open the input file." << std::endl;
//        return 1;
//    }

    // Build and initialize register
    int myRegister[8];
    for (int i = 0; i < 8; i++) {
        myRegister[i] = 0;
    }

    ifstream inputFile("program.mc");

    // Store blockSizeInWords
    int blockSizeInWords = atoi(argv[2]);

    // Store number of sets
    int numberOfSets = atoi(argv[3]);

    // Store blocks per set
    int blocksPerSetCache = atoi(argv[4]);

    // FOR DEBUG ONLY DELETE LATER
    blockSizeInWords = 4;
    numberOfSets = 2;
    blocksPerSetCache = 1;

    // Build instruction storage
    char line[MAXLINELENGTH];
    int memoryArray[NUMMEMORY];
    // Initialize array to zeroes
    for (int i = 0; i < NUMMEMORY; i++) {
        memoryArray[i] = 0;
    }
    int numMemory = 0;
    // Store numbers in the array
    while (inputFile.getline(line, sizeof(line))) {
        // Store current "line" in memory
        stringstream currentLine(line);
        int lineInt;
        currentLine >> lineInt;
        memoryArray[numMemory] = lineInt;
        // Output current line
        cout << "memory[" << numMemory << "] = " << memoryArray[numMemory] << endl;
        // Increment numMemory
        numMemory++;
    }

    // Load the primary memory
    memoryBlock* primaryMem = new memoryBlock[NUMMEMORY/blockSizeInWords];
    // Loop through all blocks
    for (int i = 0; i < NUMMEMORY/blockSizeInWords; i++) {
        // Loop through each word in each block
        primaryMem[i].data = new int [blockSizeInWords];
        for (int j = 0; j < blockSizeInWords; j++) {
            primaryMem[i].data[j] = memoryArray[i+j];
        }
    }
    // Tell each memory block which set it is a part of
    int currentSetNumber = 0;
    for (int i = 0; i < NUMMEMORY/blockSizeInWords; i++) {
        primaryMem[i].setNum = currentSetNumber;
        currentSetNumber++;
        if (currentSetNumber >= numberOfSets) {
            currentSetNumber = 0;
        }
    }

    // Begin building cache
    int cacheSize = blockSizeInWords*numberOfSets*blocksPerSetCache;\
    // Total number of blocks in the cache
    int numCacheBlocks = cacheSize/blockSizeInWords;
    // Build the cache array
    cacheBlock cacheBlockArray[numCacheBlocks];
    int setIndexIndex = 0;
    cout << "---CACHE---" << endl;
    int j = 0;
    for (int i = 0; i < numCacheBlocks; i++) {
        // Set valids to zero (bools)
        cacheBlockArray[i].validBit = false;
        // Set dirties to zero (bools)
        cacheBlockArray[i].dirtyBit = false;
        // Set tags to zero (strings)
        cacheBlockArray[i].tag = " ";
        // Set length of data array (block size)
        cacheBlockArray[i].cacheBlockDataArray = new int[blockSizeInWords]; // NEW HERE, ADD CORRESPONDING DELETE !!
        // Set time since last use to some large number
        cacheBlockArray[i].timeSinceLastUse = 999999;
        // Set "set" association
        if (i-blocksPerSetCache*j< blocksPerSetCache) {
            cacheBlockArray[i].setIndex = setIndexIndex;
        } else {
            setIndexIndex++;
            cacheBlockArray[i].setIndex = setIndexIndex;
            j++;
        }
        cout << "cache[" << i << "] setIndex:  " << cacheBlockArray[i].setIndex << endl;
    }

    // Set the tag size
    int tagSize = 16-log2(blockSizeInWords)-log2(numberOfSets);

    // Break down and process the instructions
    bool halted = false;
    int currentInstruction = 0;
    while (currentInstruction < numMemory && !halted) {
        // Save the current instruction as a 32-bit integer
        int32_t binaryInstruction(primaryMem[currentInstruction/blockSizeInWords].data[currentInstruction]);
        // Save the instruction as a bitvector (new word I made up)
        vector<bool> instructionBits = intToVector(binaryInstruction);
        // Convert the bitvector to a string
        string instructionString = vectorToString(instructionBits);

        // First, lets extract the opcode
        string opcode = instructionString.substr(7, 3);
        // Then extract RegA
        string regAString = instructionString.substr(10, 3);
        vector<bool> regAB = stringToVector(regAString);
        int regA = myRegister[vectorToInt(regAB)];
        // Then extract RegB
        string regBString = instructionString.substr(13, 3);
        vector<bool> regBB = stringToVector(regBString);
        int regB = myRegister[vectorToInt(regBB)];
        // Then extract the rest
        string offsetField = instructionString.substr(16, 16);

        // Do we have a store?
        if (opcode == "011"){
            // Calculate a new string that is the binary string of regA+offset
            vector<bool> regABits = intToVector(regA);
            vector<bool> offsetFieldBits = stringToVector(offsetField);
            int sum = regA + vectorToInt(offsetFieldBits);
            vector<bool> sourceAddressBits = intToVector(sum);
            // Put the bitvector back into a string
            string sourceAddressString = vectorToString(sourceAddressBits);
            // Put the block into cache...
            // Find LRU block
            int lruIndex = findLRU(cacheBlockArray, blocksPerSetCache * numberOfSets, primaryMem[vectorToInt(sourceAddressBits)/blockSizeInWords].setNum);
            // Now we need to replace the lru block
            // First check if it has been overwritten
            if (cacheBlockArray[lruIndex].dirtyBit == 1) {
                // Reassemble the memory address that this sucker is supposed to go to
                string tagPartOfDestAddress = cacheBlockArray[lruIndex].tag;
                vector<bool> setIndexBits = intToVector(cacheBlockArray[lruIndex].setIndex);
                string setIndexString = vectorToString(setIndexBits);
                string extra = "0";
                for (int h = 0; h < log2(blockSizeInWords); h++) {
                    extra += "0";
                }
                // Combine all the strings
                string finalOffsetField = tagPartOfDestAddress+setIndexString+extra;
                vector<bool> bits = stringToVector(finalOffsetField);
                int fieldInt = vectorToInt(bits);
                // LOG
                printAction(fieldInt/blockSizeInWords*blockSizeInWords, blockSizeInWords, cacheToMemory);
                for (int k = 0; k < blockSizeInWords; k++) {
                    primaryMem[fieldInt/blockSizeInWords].data[k] = cacheBlockArray[lruIndex].cacheBlockDataArray[k];
                }
            }
            // Now we can safely overwrite it
            // LOG
            printAction(regB, blockSizeInWords, memoryToCache);
            for (int k = 0; k < blockSizeInWords; k++) {
                cacheBlockArray[lruIndex].cacheBlockDataArray[k] = primaryMem[vectorToInt(sourceAddressBits)/blockSizeInWords].data[k];
            }
            cacheBlockArray[lruIndex].setIndex = primaryMem[vectorToInt(sourceAddressBits)/blockSizeInWords].setNum;
            cacheBlockArray[lruIndex].dirtyBit = 1;
            cacheBlockArray[lruIndex].validBit = 1;
            cacheBlockArray[lruIndex].timeSinceLastUse = 0;
            cacheBlockArray[lruIndex].tag = sourceAddressString.substr(0,tagSize);
            // Put the register's data in place
            string bitOffset = sourceAddressString.substr(tagSize+log2(numberOfSets),log2(blockSizeInWords));
            vector<bool> bits = stringToVector(bitOffset);
            int overwriteLocation = vectorToInt(bits);
            vector<bool> regLocation = intToVector(regB);
            cacheBlockArray[lruIndex].cacheBlockDataArray[overwriteLocation] = myRegister[vectorToInt(regLocation)];
            // LOG
            printAction(lruIndex, 1, cacheToProcessor);
        }

        // Do we have a load?
        else if (opcode == "010") {
            // Check if the address is in the cache
            // Add regA to offsetField and convert back to string
            vector<bool> offsetFieldBits = stringToVector(offsetField);
            int sourceAddressIndex = regA+ vectorToInt(offsetFieldBits);
            // Put the bitvector back into a string
            vector<bool> sourceAddressBits = intToVector(sourceAddressIndex);
            string sourceAddressString = vectorToString(sourceAddressBits);
            // Extract tag from source address string
            string instTag = sourceAddressString.substr(0, tagSize);
            // Bool to track whether something was found in the cache
            bool foundInCache = false;
            int setIndexLength = log2(numberOfSets);
            int i = 0;
            while (i < numberOfSets*blocksPerSetCache && !foundInCache) {
                if (cacheBlockArray[i].validBit && cacheBlockArray[i].tag == instTag) {
                    // Check if the set index from our source address matches that of the current cacheBlock
                    // Extract the substring for the setIndex from the source address
                    string indexString = sourceAddressString.substr(tagSize, setIndexLength);
                    vector<bool> bits = stringToVector(indexString);
                    int indexStringInt = vectorToInt(bits);
                    if (cacheBlockArray[i].setIndex == indexStringInt) {
                        // It's a hit
                        // Find the block offset
                        string blockOffset = sourceAddressString.substr(tagSize + setIndexIndex, 16);
                        vector<bool> bits2 = stringToVector(blockOffset);
                        int blockOffsetInt = vectorToInt(bits2);
                        // Locate necessary content and store in the necessary register
                        vector<bool> bits3 = intToVector(regB);
                        int regBInt = vectorToInt(bits3);
                        myRegister[regBInt] = cacheBlockArray[i].cacheBlockDataArray[blockOffsetInt];
                        // Update time since last used
                        cacheBlockArray[i].timeSinceLastUse = 0;
                        // Indicate that it was found
                        foundInCache = true;
                    }
                }
                i++;
            }
            if (!foundInCache) {
                // Locate the data in memory
                vector<bool> bits = stringToVector(sourceAddressString);
                int sourceAddressInt = vectorToInt(bits);
                int sourceAddress = sourceAddressInt;
                // Find which block its in
                int sourceBlock = sourceAddress / blockSizeInWords;
                // Find least recently used block OF THAT SET in cache
                int lruBlockIndex = findLRU(cacheBlockArray, blocksPerSetCache * numberOfSets, primaryMem[sourceBlock].setNum);

                if (cacheBlockArray[lruBlockIndex].dirtyBit == 1) {
                    // Reassemble the memory address that this sucker is supposed to go to
                    string tagPartOfDestAddress = cacheBlockArray[lruBlockIndex].tag;
                    vector<bool> setIndexBits = intToVector(cacheBlockArray[lruBlockIndex].setIndex);
                    string setIndexString = vectorToString(setIndexBits);
                    string extra = "0";
                    for (int h = 0; h < log2(blockSizeInWords); h++) {
                        extra += "0";
                    }
                    // Combine all the strings
                    string finalOffsetField = tagPartOfDestAddress+setIndexString+extra;
                    vector<bool> fieldBits = stringToVector(finalOffsetField);
                    int fieldInt = vectorToInt(fieldBits);
//                    int fieldInt = stringToInt(finalOffsetField);
                    // LOG
                    printAction(lruBlockIndex*blockSizeInWords, blockSizeInWords, cacheToMemory);
                    for (int k = 0; k < blockSizeInWords; k++) {
                        primaryMem[fieldInt/blockSizeInWords].data[k] = cacheBlockArray[lruBlockIndex].cacheBlockDataArray[k];
                    }
                }

                // LOG
                printAction(sourceBlock*blockSizeInWords, blockSizeInWords, memoryToCache);

                // Replace the block in the cache
                bool oldValidBit = cacheBlockArray[lruBlockIndex].validBit;
                cacheBlockArray[lruBlockIndex].setIndex = primaryMem[sourceBlock].setNum;
                cacheBlockArray[lruBlockIndex].validBit = 1;
                cacheBlockArray[lruBlockIndex].dirtyBit = 0;
                cacheBlockArray[lruBlockIndex].timeSinceLastUse = 0;
                // Copy data
                for (int k = 0; k < blockSizeInWords; k++) {
                    cacheBlockArray[lruBlockIndex].cacheBlockDataArray[k] = primaryMem[sourceBlock].data[k];
                }
                // Set tag
                cacheBlockArray[lruBlockIndex].tag = instTag;
            }
        }

        // Do we have a halt?
        else if (opcode == "110"){
            halted = true;
        }
        currentInstruction++;
    }

    delete [] primaryMem;
    for (int i = 0; i < numberOfSets*blocksPerSetCache; i++) {
        delete [] cacheBlockArray[i].cacheBlockDataArray;
    }


    return 0;
}

string vectorToString(const vector<bool> &v) {
    string output;
    for (int i = 0; i < v.size(); i++) {
        if (v[v.size() - i - 1]) {
            output += '1';
        } else {
            output += '0';
        }
    }
    return output;
}

vector<bool> stringToVector(const string &s) {
    vector<bool> outputVector;
    for (int i = 0; i < s.length(); i++) {
        outputVector.push_back(s[i]=='1');
    }
    return outputVector;
}

vector<bool> intToVector(int num) {
    if (num == 0) {
        return {false};
    }

    vector<bool> binaryVector;

    while (num > 0) {
        binaryVector.insert(binaryVector.begin(), num & 1);
        num >>= 1;
    }

    return binaryVector;
}

int vectorToInt(const std::vector<bool>& binaryVector) {
    int result = 0;

    // Iterate through each bit in the vector
    for (int i = 0; i < binaryVector.size(); i++) {
        result = (result << 1) | static_cast<int>(binaryVector[i]);
    }

    return result;
}

// Return the index of the LRU memory chunk
int findLRU(cacheBlock* cacheBlockArray, int numCacheBlocks, int setNum) {
    int maxVal = 0;
    int maxValIndex = 0;
    for (int i = 0; i < numCacheBlocks; i++){
        if (cacheBlockArray[i].timeSinceLastUse > maxVal && cacheBlockArray[i].setIndex == setNum && cacheBlockArray[i].validBit){
            maxVal = cacheBlockArray[i].timeSinceLastUse;
            maxValIndex = i;
        }
    }
    return maxValIndex;
}
