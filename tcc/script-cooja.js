load("nashorn:mozilla_compat.js");

importPackage(java.io);

WAIT_UNTIL(id == 1 && msg.contains("Starting to sense"));

outputs = new Object();
runId = "10-";
fileId = "10-duty-cycle";
fileLocation = "/home/jonathan/contiki-aloha/tcc/simulation-runs/" + fileId + "/log_" + runId + fileId + ".csv";

// In milliseconds.
TIMEOUT(7200000);

while (true) {
  //Has the output file been created.
  if (id == 1 && !msg.contains("Starting to sense")) {
    if (!outputs[id.toString()]) {
      outputs[id.toString()] = new FileWriter(fileLocation);
      outputs[id.toString()].write("time,_Eihop,_P0,hops,d,_R,_Nb\n");
    }
    //Write to file.
    outputs[id.toString()].write(msg + "\n");
    log.log(msg + "\n");
  }

  try {
    //This is the tricky part. The Script is terminated using
    // an exception. This needs to be caught.
    YIELD();
  } catch (e) {
    //Close files.
    for (var ids in outputs) {
      outputs[ids].close();
    }
    //Rethrow exception again, to end the script.
    throw ('test script killed');
  }
}