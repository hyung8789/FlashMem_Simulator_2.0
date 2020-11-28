# FlashMem_Simulator_2.0

<img src="/res/pgb-flashmemory.png" width="200" height="133"><br></br>
NAND Flash Memory Simulater for Block level Mapping Method with 2 Types of Mapping Table, Hybrid Mapping Method (Log algorithm - 1:2 Block level mapping with Dynamic Table)

	1) Static Table : Mapping table initially all correspond 1 : 1 (Logical Sector or Block Num -> Physical Sector or Block Num)
	2) Dynamic Table : Mapping table initially all empty (non-assigned)

※ Additional implementation

	- Garbage Collecter
		=> Because the cost of erasing operations is greater than reading and writing, 
		freeing up space for recording during idle time or through garbage collection at an appropriate time.

	- Trace function
		=> Performance test through write operation by specific pattern.	
<br></br>

<h3><strong>< Build Option ></strong></h3><br>
- Used __int64(long long) data type for exception-handling from user input. Therefore, the solution platform is built for x64 at build time.
- Modify this predefined macro as you want (Refer to "Build_Options.h")
- Exception Handling for set SPARE_BLOCK_RATIO and VICTIM_BLOCK_QUEUE_RATIO to values in different ratio : not-implemented (Refer to "Build_Options.h")

| Predefined Macro | For What |
|:---:|:---|
| DEBUG_MODE | Trace all possible error situations (0 : Not Use, 1 : Use) |
| BLOCK_TRACE_MODE | Trace wear-leveling per block for all physical blocks (0 : Not Use, 1 : Use) |
| SPARE_BLOCK_RATIO | The rate of spare blocks to be managed by the system for the total number of blocks (Blocks that cannot be record data directly) (Default : 0.08, 8%) |
| VICTIM_BLOCK_QUEUE_RATIO | Set the size of the Victim Block Queue to the ratio size of the total number of blocks in the generated flash memory (Default : 0.08, 8%) |

<br></br>

<h3><strong>< Normal Mode (with not use Any Mapping Method) Command List ></strong></h3>

| Command | Action |
|:---:|:---|
| init x or i x | Create x MB Storage File |
| read PSN or r PSN | Read data at Physical Sector Num(PSN) Position |
| write PSN data or w PSN data | Write data at Physical Sector Num(PSN) Position |
| erase PBN or e PBN | Erase data at Physical Block Num(PBN) Position |
| change | Change Mapping Method |
| clrglobalcnt | Clear Global Flash Memory Operation Count |
| pbninfo PBN | Output meta information for all sectors of PBN |
| info | Output Current flash memory information |
| exit | Terminate Program |

<br></br>

<h3><strong>< Block Mapping Method, Hybrid Mapping Method Command List ></strong></h1>

| Command | Action |
|:---:|:---|
| init x or i x | Create x MB Storage File |
| read LSN or r LSN | Read data at Logical Sector Num(LSN) Position |
| write LSN data or w LSN data | Write data at Logical Sector Num(LSN) Position |
| change | Change Mapping Method |
| print | Print Mapping Table(LSN -> PSN or LBN -> PBN) |
| searchmode | Change Algorithm for finding empty sector(offset) in block |
| trace | Write test with "W [TAB] LSN" format file |
| clrglobalcnt | Clear Global Flash Memory Operation Count (No dependency on each other between Per Block trace count) |
| vqprint | Output Current Victim Block Queue |
| lbninfo LBN | Output meta information for all sectors of LBN |
| pbninfo PBN | Output meta information for all sectors of PBN |
| info | Output Current flash memory information |
| exit | Terminate Program after empty the Victim Queue by GC |

<br></br>

<h3><strong>< Bugs and needs to be improvement ></strong></h3><br>
1) In Hybrid Mapping, if all physical spaces are written with valid data, no more new data can be written but overwrite action must can be performed in current data<br>
(Because Log Block(PBN2) cannot aligned to LBN with no empty spaces(no existing empty physical block), so copy valid data from Data Block(PBN1) to Spare Block and perform Erase operation at PBN1, write new data to Spare Block, set Spare Block to normal Block, current PBN1 set to Spare Block)<br>
2) Invalid page calculation error on Hybrid Mapping - Handling exception or modify logic <br>
3) According to Invalid ratio threshold, calculate LBN's invalid ratio at proper time (Hybrid Mapping)<br>
4) Search Algorithm improvement for finding empty sector(offset) in a block(For mapping technique that using page-by-page mapping : Hybrid Mapping) : Divide and Conquer - binary search(Refer to : https://github.com/hyung8789/Search_Algorithm_for_FlashMem)<br>
5) Spare Area def,imp refactoring for more reusable. (Refer to : https://github.com/hyung8789/Spare_Area_model_for_flashmem)<br>
<br></br>

<h3><strong>< References ></strong></h3><br>
- A survey of Flash Translation Layer : <br>https://www.sciencedirect.com/science/article/abs/pii/S1383762109000356<br>
- A SPACE-EFFICIENT FLASH TRANSLATION LAYER FOR COMPACTFLASH SYSTEMS : <br>https://ieeexplore.ieee.org/document/1010143<br>
- A Log Buffer-Based Flash Translation Layer Using Fully-Associative Sector Translation : https://dl.acm.org/doi/10.1145/1275986.1275990<br>
- Micron Technical Note - Garbage Collection : <br>https://www.micron.com/-/media/client/global/Documents/Products/Technical%20Note/NAND%20Flash/tn2960_garbage_collection_slc_nand.ashx<br>
