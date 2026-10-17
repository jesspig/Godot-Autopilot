#!/usr/bin/env bun
import { main } from "../src/main";

process.exit(await main(process.argv.slice(2)));
