/* Copyright 2015 Bloq Inc.
 * Distributed under the MIT/X11 software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include "pgdb2-config.h"

#include <assert.h>
#include <pgdb2.h>

namespace page {

bool DB::get(const std::string& key, std::string& valueOut)
{
	if (!running)
		return false;	// not found

	// Start search at root directory
	uint32_t dir_ino = DBINO_ROOT_DIR;
	bool recurse = false;

	// Loop through successive directories as needed
	while (recurse) {

		// Read directory
		Dir dir;
		readDir(dir_ino, dir);

		// Loop through directory entry keys
		for (size_t i = 0; i < dir.ents.size(); i++) {
			const DirEntry& ent = dir.ents[i];

			// ASCII compare search key to dirent key
			int cmpres = key.compare(ent.key);

			// Key entry larger: Search key not in db
			if (cmpres < 0) 
				return false;	// not found

			// Case 1: Matched key inside key range
			else if ((ent.d_type == DE_DIR) &&
			    (key.compare(ent.key_end) <= 0)) {
				recurse = true;
				dir_ino = ent.ino_idx;
				continue;
			}

			// Case 2: Matched key, value in dirent
			else if ((ent.d_type == DE_KEY_VALUE) &&
				 (cmpres == 0)) {
				valueOut = ent.value;
				return true;	// found
			}

			// Case 3: Matched key, value in inode
			else if ((ent.d_type == DE_KEY) &&
				 (cmpres == 0)) {
				// Read data from inode
				std::vector<unsigned char> buf;
				readInodeData(ent.ino_idx, buf);

				// Return data
				valueOut.assign(buf.begin(),buf.end());
				return true;	// found
			}

			// Otherwise, continue searching through directory
			assert(cmpres > 0);
		}

		break;	// fall through to 'not found'
	}

	return false;	// not found
}

}
