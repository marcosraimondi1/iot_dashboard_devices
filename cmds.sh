# first get device credentials
curl \
	-H "Content-type: application/x-www-form-urlencoded" \
	-d "dId=1&password=GAGX2hgDZ9" \
	-X POST localhost:3001/api/getdevicecredentials

# credentials change after certain delay, so you need to connect fast

# with mosquitto clients
# - subscribe
mosquitto_sub --url mqtt://username:password@host:port/topic
# example: subscribe to all actuators from device
mosquitto_sub --url mqtt://HuGFf0WiYR:n8ayKSgIq4@localhost:1883/67923ede494488f95645b3aa/1/+/actdata

# - publish
mosquitto_pub --url mqtt://username:password@host:port/topic -m 'message'
# example: send msg from certain sensor variable
mosquitto_pub --url mqtt://HuGFf0WiYR:n8ayKSgIq4@localhost:1883/67923ede494488f95645b3aa/1/hJo2gk3hfX/sdata -m '{"value": true}'

